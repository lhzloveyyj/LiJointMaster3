#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

/**
 * @file plotmanager.h
 * @brief 实时图表管理器类声明
 *
 * 基于 QCustomPlot 的实时数据曲线管理器。
 * 提供：
 * - 按名称注册曲线
 * - 实时追加数据（自动推进时间轴）
 * - 定时重绘（30ms 间隔）
 * - 鼠标悬停数据点吸附读数
 * - 横轴范围控制
 */

#include <QObject>
#include <QMap>
#include <QTimer>
#include "third_party/qcustomplot/qcustomplot.h"

/**
 * @brief 图表管理器
 *
 * 封装 QCustomPlot 的曲线注册、数据追加和交互功能。
 * 内部维护一个时间键 m_key，每次追加数据时自动递增，
 * 实现实时滚动效果。
 *
 * 使用示例：
 * @code
 * auto *plotMgr = new PlotManager(m_plotWidget, this);
 * plotMgr->addGraph("speed", Qt::yellow);
 * plotMgr->appendData("speed", 123.45);
 * @endcode
 */
class PlotManager : public QObject
{
    Q_OBJECT
public:
    /**
     * @param plotWidget  所属的 QCustomPlot 对象
     * @param parent      父 QObject
     */
    explicit PlotManager(QCustomPlot *plotWidget, QObject *parent = nullptr);

    /**
     * @brief 注册一条新曲线
     * @param name  曲线名称（追加数据时通过此名称引用）
     * @param color 曲线颜色
     */
    void addGraph(const QString &name, const QColor &color);

    /**
     * @brief 设置横轴可视范围
     * @param rangeSec 可视窗口的秒数（0.1 ~ 10s）
     */
    void setXAxisRange(double rangeSec);

public slots:
    /**
     * @brief 向指定的曲线追加一个数据点
     *
     * 如果曲线不存在或当前处于暂停状态，静默忽略。
     * 每次调用自动推进时间轴 m_key += 0.0005。
     *
     * @param name  曲线名称
     * @param value 数据值
     */
    void appendData(const QString &name, double value);

    /**
     * @brief 暂停/恢复曲线更新（纯本地操作，不涉及下位机）
     *
     * 暂停后 appendData() 不再追加新数据点，图表冻结。
     *
     * @param paused true 暂停，false 恢复
     */
    void setPaused(bool paused);

private slots:
    /** @brief 鼠标移动事件处理（数据点吸附提示） */
    void onMouseMove(QMouseEvent *event);

private:
    QCustomPlot *m_plot;              ///< QCustomPlot 绘图控件指针
    QTimer *m_replotTimer;            ///< 定时重绘定时器（30ms）

    /** @brief 曲线数据结构 */
    struct GraphData {
        QCPGraph *graph;              ///< QCustomPlot 图形对象
        QVector<double> data;         ///< 数据点缓存（暂未使用）
    };

    QMap<QString, GraphData> m_graphs; ///< 曲线名称 → 数据映射表
    double m_key;                      ///< 当前时间键值（每次追加自增 0.0005）
    double m_xAxisRange;               ///< X 轴可视范围（秒）

    bool m_paused = false;            ///< 是否暂停曲线更新

    QCPItemText *m_tipText;           ///< 鼠标悬停时的数据值提示文本
};

#endif // PLOTMANAGER_H
