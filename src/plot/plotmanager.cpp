#include "plotmanager.h"

/**
 * @file plotmanager.cpp
 * @brief 实时图表管理器实现
 *
 * 基于 QCustomPlot 的实时曲线绘制模块。
 * 核心机制：
 * - 按名称注册曲线，支持多曲线同时显示
 * - 内部维护单调递增的时间键 m_key（步进 0.0005）
 * - 30ms 定时器批量重绘，降低 CPU 占用
 * - 鼠标悬停时自动吸附到最近数据点并显示 Y 值
 * - 可调节的横轴可视范围（0.1s ~ 10s）
 */

#include <cmath>

PlotManager::PlotManager(QCustomPlot *plotWidget, QObject *parent)
    : QObject(parent),
    m_plot(plotWidget),
    m_key(0),           // 时间轴从 0 开始
    m_xAxisRange(5)     // 默认显示 5 秒窗口
{
    // ---- 坐标轴样式配置 ----
    // X 轴标签固定为 "Time (s)"，Y 轴标签已移除（曲线名称由 UI 示意）
    m_plot->xAxis->setLabel("Time (s)");
    m_plot->yAxis->setLabel(QString());

    // 坐标轴和刻度颜色：白色
    m_plot->xAxis->setBasePen(QPen(Qt::white));
    m_plot->yAxis->setBasePen(QPen(Qt::white));
    m_plot->xAxis->setTickPen(QPen(Qt::white));
    m_plot->yAxis->setTickPen(QPen(Qt::white));
    m_plot->xAxis->setTickLabelColor(Qt::white);
    m_plot->yAxis->setTickLabelColor(Qt::white);

    // 网格颜色：深灰色（在深色背景上减弱视觉干扰）
    m_plot->xAxis->grid()->setPen(QPen(QColor(80, 80, 80)));
    m_plot->yAxis->grid()->setPen(QPen(QColor(80, 80, 80)));

    // ---- 鼠标交互 ----
    // 启用鼠标拖拽平移（iRangeDrag）和滚轮缩放（iRangeZoom）
    m_plot->setInteraction(QCP::iRangeDrag);
    m_plot->setInteraction(QCP::iRangeZoom);
    m_plot->setMouseTracking(true);  // 启用鼠标追踪以接收 mouseMove

    // ---- 鼠标数值提示文本 ----
    // 在图表左上角显示一个半透明黑底的 Y 值标签
    m_tipText = new QCPItemText(m_plot);
    m_tipText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_tipText->setPadding(QMargins(4,4,4,4));
    m_tipText->setBrush(QColor(0, 0, 0, 160));  // 半透明黑色背景
    m_tipText->setColor(Qt::white);
    m_tipText->setVisible(false);  // 默认隐藏

    // 鼠标移动事件连接
    connect(m_plot, &QCustomPlot::mouseMove,
            this, &PlotManager::onMouseMove);

    // ---- 定时重绘 ----
    // 30ms ≈ 33fps，平衡 CPU 占用和刷新流畅度
    m_replotTimer = new QTimer(this);
    m_replotTimer->setInterval(30);
    connect(m_replotTimer, &QTimer::timeout, [this]() {
        m_plot->replot();
    });
    m_replotTimer->start();
}

/**
 * @brief 注册一条新的图表曲线
 *
 * 如果同名曲线已存在则静默忽略（避免重复注册）。
 * 每条曲线自动分配一种颜色（由调用方指定）。
 *
 * @param name  曲线标识名
 * @param color 曲线颜色
 */
void PlotManager::addGraph(const QString &name, const QColor &color)
{
    if (m_graphs.contains(name)) return;

    QCPGraph *graph = m_plot->addGraph();
    graph->setPen(QPen(color));

    m_graphs[name] = { graph, QVector<double>() };
}

/**
 * @brief 向指定曲线追加一个数据点
 *
 * 时间轴自动推进 m_key += 0.0005（约 1/2000 秒每点）。
 * 每次追加后 X 轴以右对齐方式滚动到最新数据位置。
 *
 * @param name  曲线名称（必须在 addGraph 中注册过）
 * @param value 数据点的 Y 值
 */
void PlotManager::appendData(const QString &name, double value)
{
    if (m_paused || !m_graphs.contains(name)) return;

    m_key += 0.0005;

    GraphData &g = m_graphs[name];
    g.data.append(value);

    g.graph->addData(m_key, value);

    // X 轴右对齐：保持 m_key 在可视窗口右端
    m_plot->xAxis->setRange(m_key, m_xAxisRange, Qt::AlignRight);
}

void PlotManager::setPaused(bool paused)
{
    m_paused = paused;
}

/**
 * @brief 设置横轴可视范围
 *
 * @param seconds 视窗宽度（秒）
 */
void PlotManager::setXAxisRange(double seconds)
{
    m_xAxisRange = seconds;
    m_plot->xAxis->setRange(m_key - seconds, m_key);
}

/**
 * @brief 鼠标移动事件——数据点吸附提示
 *
 * 当鼠标在图表上移动时：
 * 1. 遍历所有曲线，使用二分查找找到最接近鼠标 X 位置的数据点
 * 2. 计算像素距离并记录最近的点
 * 3. 如果最近点在 25 像素阈值内，显示 Y 值提示
 * 4. 否则隐藏提示
 *
 * @param event 鼠标事件
 */
void PlotManager::onMouseMove(QMouseEvent *event)
{
    double mouseX = m_plot->xAxis->pixelToCoord(event->pos().x());
    double mouseY = m_plot->yAxis->pixelToCoord(event->pos().y());

    const double SNAP_PIXEL = 25;   // 数据点吸附的像素阈值
    bool found = false;
    double bestPixelDist = 1e9;
    double bestKey = 0, bestVal = 0;

    // 遍历所有曲线，寻找离鼠标最近的数据点
    for (auto &g : m_graphs)
    {
        QCPGraph *graph = g.graph;
        auto container = graph->data();
        if (!container || container->isEmpty()) continue;

        // 使用 QCPGraph 的二分查找接口找到最接近 mouseX 的点
        auto it = container->findBegin(mouseX, true);   // lower_bound
        if (it != container->constEnd())
        {
            const QCPGraphData &d = *it;
            double px = m_plot->xAxis->coordToPixel(d.key);
            double py = m_plot->yAxis->coordToPixel(d.value);
            double dist = std::hypot(px - event->pos().x(), py - event->pos().y());

            if (dist < bestPixelDist)
            {
                bestPixelDist = dist;
                bestKey = d.key;
                bestVal = d.value;
                found = true;
            }
        }

        // 同时也检查前一个点（如果存在）
        if (it != container->constBegin())
        {
            --it;
            const QCPGraphData &d = *it;
            double px = m_plot->xAxis->coordToPixel(d.key);
            double py = m_plot->yAxis->coordToPixel(d.value);
            double dist = std::hypot(px - event->pos().x(), py - event->pos().y());

            if (dist < bestPixelDist)
            {
                bestPixelDist = dist;
                bestKey = d.key;
                bestVal = d.value;
                found = true;
            }
        }
    }

    // 如果最近点超过吸附阈值，隐藏提示
    if (!found || bestPixelDist > SNAP_PIXEL)
    {
        m_tipText->setVisible(false);
        m_plot->replot();
        return;
    }

    // 显示吸附点的 Y 值（保留 2 位小数）
    m_tipText->setText(QString("Y: %1").arg(bestVal, 0, 'f', 2));

    // 提示文本显示在吸附点左上方偏移 20px 处
    int px = m_plot->xAxis->coordToPixel(bestKey) - 20;
    int py = m_plot->yAxis->coordToPixel(bestVal) - 20;

    if (px < 0) px = 0;
    if (py < 0) py = 0;

    m_tipText->position->setType(QCPItemPosition::ptAbsolute);
    m_tipText->position->setCoords(px, py);
    m_tipText->setVisible(true);

    m_plot->replot();
}
