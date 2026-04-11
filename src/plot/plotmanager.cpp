#include "plotmanager.h"

#include <cmath>

PlotManager::PlotManager(QCustomPlot *plotWidget, QObject *parent)
    : QObject(parent),
    m_plot(plotWidget),
    m_key(0),
    m_xAxisRange(5)
{
    // ---- 坐标轴样式 ----
    m_plot->xAxis->setLabel("Time (s)");
    m_plot->yAxis->setLabel(QString());

    m_plot->xAxis->setBasePen(QPen(Qt::white));
    m_plot->yAxis->setBasePen(QPen(Qt::white));
    m_plot->xAxis->setTickPen(QPen(Qt::white));
    m_plot->yAxis->setTickPen(QPen(Qt::white));

    m_plot->xAxis->setTickLabelColor(Qt::white);
    m_plot->yAxis->setTickLabelColor(Qt::white);

    m_plot->xAxis->grid()->setPen(QPen(QColor(80, 80, 80)));
    m_plot->yAxis->grid()->setPen(QPen(QColor(80, 80, 80)));

    // ---- 鼠标交互 ----
    m_plot->setInteraction(QCP::iRangeDrag);
    m_plot->setInteraction(QCP::iRangeZoom);
    m_plot->setMouseTracking(true);

    // ---- 鼠标数值提示文本 ----
    m_tipText = new QCPItemText(m_plot);
    m_tipText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_tipText->setPadding(QMargins(4,4,4,4));
    m_tipText->setBrush(QColor(0, 0, 0, 160));
    m_tipText->setColor(Qt::white);
    m_tipText->setVisible(false);

    connect(m_plot, &QCustomPlot::mouseMove,
            this, &PlotManager::onMouseMove);

    // ---- 定时刷新 ----
    m_replotTimer = new QTimer(this);
    m_replotTimer->setInterval(30);
    connect(m_replotTimer, &QTimer::timeout, [this]() {
        m_plot->replot();
    });
    m_replotTimer->start();
}

void PlotManager::addGraph(const QString &name, const QColor &color)
{
    if (m_graphs.contains(name)) return;

    QCPGraph *graph = m_plot->addGraph();
    graph->setPen(QPen(color));

    m_graphs[name] = { graph, QVector<double>() };
}

void PlotManager::appendData(const QString &name, double value)
{
    if (!m_graphs.contains(name)) return;

    m_key += 0.0005;

    GraphData &g = m_graphs[name];
    g.data.append(value);

    g.graph->addData(m_key, value);

    m_plot->xAxis->setRange(m_key, m_xAxisRange, Qt::AlignRight);
}

void PlotManager::setXAxisRange(double seconds)
{
    m_xAxisRange = seconds;
    m_plot->xAxis->setRange(m_key - seconds, m_key);
}

void PlotManager::onMouseMove(QMouseEvent *event)
{
    double mouseX = m_plot->xAxis->pixelToCoord(event->pos().x());
    double mouseY = m_plot->yAxis->pixelToCoord(event->pos().y());

    const double SNAP_PIXEL = 25;   // 吸附像素距离
    bool found = false;
    double bestPixelDist = 1e9;
    double bestKey = 0, bestVal = 0;

    for (auto &g : m_graphs)
    {
        QCPGraph *graph = g.graph;
        auto container = graph->data();
        if (!container || container->isEmpty()) continue;

        // ---- 使用二分查找找到最接近 mouseX 的点 ----
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

        // 看前一个点（upper）
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

    // ---- 超出吸附距离，不显示 ----
    if (!found || bestPixelDist > SNAP_PIXEL)
    {
        m_tipText->setVisible(false);
        m_plot->replot();
        return;
    }

    // ---- 显示吸附后的 Y 值 ----
    m_tipText->setText(QString("Y: %1").arg(bestVal, 0, 'f', 2));

    // 显示在左上
    int px = m_plot->xAxis->coordToPixel(bestKey) - 20;
    int py = m_plot->yAxis->coordToPixel(bestVal) - 20;

    if (px < 0) px = 0;
    if (py < 0) py = 0;

    m_tipText->position->setType(QCPItemPosition::ptAbsolute);
    m_tipText->position->setCoords(px, py);
    m_tipText->setVisible(true);

    m_plot->replot();
}
