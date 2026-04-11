#ifndef WIDGET_H
#define WIDGET_H

#include <QEvent>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QVariantList>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QDialog;
class QLabel;
class QLineEdit;
class PlotManager;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTextEdit;
class QVBoxLayout;
class QCustomPlot;
class SerialManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct ThemePalette {
        QString windowBg;
        QString panelBg;
        QString panelBorder;
        QString sectionBg;
        QString textPrimary;
        QString textSecondary;
        QString chartBg;
    };

    void buildUi();
    void buildTopBar(QVBoxLayout *mainV);
    void buildBody(QVBoxLayout *mainV);
    void buildSerialPanel(QWidget *container);
    void buildRightPanel(QWidget *container);
    void buildModeSensePage(QWidget *page);
    void buildModeNoSensePage(QWidget *page);
    void buildSettingsDialog();

    void applyTheme(int themeIndex);
    void bindSignals();

    void refreshPortsToUi();
    void appendLog(const QString &line);
    double parseField(QLineEdit *edit) const;
    void sendCommand(int command, QLineEdit *field = nullptr);
    void updateTrendCommand(int openCommand, bool checked);
    void setupPlotGraphs();
    void appendTrendValues(int command, const QVariantList &values);
    void backfillConnectPayload(const QVariantList &values);
    void applyZeroCalibrationPayload(const QVariantList &values);

    ThemePalette currentTheme() const;

    Ui::Widget *ui;
    SerialManager *m_serial;

    QComboBox *m_modeCombo;
    QPushButton *m_settingsBtn;
    QStackedWidget *m_modeStack;

    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QPushButton *m_openCloseBtn;
    QLabel *m_statusLabel;
    QPushButton *m_logBtn;
    QLabel *m_motorLamp;
    QLabel *m_serialLamp;
    QPushButton *m_motorConnectBtn;
    QLineEdit *m_polePairsEdit;
    QLineEdit *m_angleDirEdit;
    QLineEdit *m_speedDirEdit;
    QLabel *m_mosTempText;
    QSlider *m_mosTempSlider;

    QLineEdit *m_zeroOffsetEdit;
    QLineEdit *m_elecAngleEdit;
    QPushButton *m_zeroCalibBtn;

    QComboBox *m_ctrlModeCombo;
    QHash<QPushButton *, int> m_trendOpenCmd;
    QHash<QPushButton *, int> m_trendCloseCmd;
    QButtonGroup *m_trendGroup;

    QHash<int, QLineEdit *> m_commandEdits;
    QHash<QString, QLineEdit *> m_pidEdits;

    QDialog *m_logDialog;
    QTextEdit *m_logText;
    QCustomPlot *m_plotWidget;
    PlotManager *m_plotManager;
    QSlider *m_plotRangeSlider;
    QDialog *m_settingsDialog;
    QComboBox *m_themeCombo;
    QWidget *m_dragHandle;
    bool m_dragging;
    QPoint m_dragOffset;

    int m_themeIndex;
    int m_accentIndex;
    bool m_themeRebuildScheduled;
};

#endif // WIDGET_H
