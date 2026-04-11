#ifndef PLOTMANAGER_H
#define PLOTMANAGER_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include "qcustomplot.h"

class PlotManager : public QObject
{
    Q_OBJECT
public:
    explicit PlotManager(QCustomPlot *plotWidget, QObject *parent = nullptr);

    void addGraph(const QString &name, const QColor &color);
    void setXAxisRange(double rangeSec);

public slots:
    void appendData(const QString &name, double value);

private slots:
    void onMouseMove(QMouseEvent *event);

private:
    QCustomPlot *m_plot;
    QTimer *m_replotTimer;

    struct GraphData {
        QCPGraph *graph;
        QVector<double> data;
    };

    QMap<QString, GraphData> m_graphs;
    double m_key;
    double m_xAxisRange;

    QCPItemText *m_tipText;   ///< 鼠标提示文本
};

#endif // PLOTMANAGER_H
