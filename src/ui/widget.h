#ifndef WIDGET_H
#define WIDGET_H

/**
 * @file widget.h
 * @brief FOC 电机上位机主窗口类声明
 *
 * 这是 LiJointMaster3 的核心界面类，继承自 QWidget。
 * 实现无边框窗口、自由布局的 FOC 调试面板，包括：
 * - 自定义标题栏（拖动、关闭、模式切换）
 * - 左侧面板：串口配置 + 电机参数
 * - 右侧面板：实时图表 + 波形控制 + 调试控制
 * - 6 主题 × 4 强调色的主题引擎
 * - 串口通信协议的 UI 绑定
 */

#include <QEvent>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QVariantList>
#include <QWidget>

/* 前向声明减少头文件依赖 */
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

/**
 * @brief 主窗口类
 *
 * 负责整个 FOC 上位机的界面搭建、信号绑定和数据流路由。
 * 使用无边框窗口（Qt::FramelessWindowHint）实现自定义标题栏。
 *
 * 主要交互流程：
 * 用户操作界面 → 信号/槽 → SerialManager 发送命令帧 →
 * 下位机响应 → 回包解析 → frameParsed 信号 →
 * UI 更新（参数回填/波形刷新）
 */
class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

protected:
    /**
     * @brief 事件过滤器
     *
     * 用于捕获标题栏（m_dragHandle）上的鼠标事件，
     * 实现无边框窗口的拖动逻辑。
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /** @brief 主题调色板结构 */
    struct ThemePalette {
        QString windowBg;       ///< 窗口背景色
        QString panelBg;        ///< 面板背景色
        QString panelBorder;    ///< 面板边框色
        QString sectionBg;      ///< 分区背景色
        QString textPrimary;    ///< 主文本色
        QString textSecondary;  ///< 副文本色
        QString chartBg;        ///< 图表背景色
    };

    /* ========== UI 构建 ========== */
    void buildUi();                                    ///< 重建整个界面（主题切换时调用）
    void buildTopBar(QVBoxLayout *mainV);              ///< 构建顶部标题栏
    void buildBody(QVBoxLayout *mainV);                ///< 构建主体内容
    void buildSerialPanel(QWidget *container);         ///< 构建左侧串口配置面板
    void buildRightPanel(QWidget *container);          ///< 构建右侧面板
    void buildSettingsDialog();                        ///< 构建设置对话框

    /* ========== 主题与信号 ========== */
    void applyTheme(int themeIndex);                   ///< 应用主题（暂未独立实现）
    void bindSignals();                                ///< 绑定信号/槽（已内联到 buildUi）

    /* ========== UI 工具方法 ========== */
    void refreshPortsToUi();                           ///< 刷新串口列表到下拉框
    void appendLog(const QString &line);               ///< 追加日志（已禁用）
    double parseField(QLineEdit *edit) const;          ///< 解析输入框数值
    void sendCommand(int command, QLineEdit *field);   ///< 发送带输入框数值的命令
    void updateTrendCommand(int openCommand, bool checked);  ///< 更新波形订阅状态

    /* ========== 图表相关 ========== */
    void setupPlotGraphs();                            ///< 注册所有图表曲线
    void appendTrendValues(int command, const QVariantList &values);  ///< 根据命令将数据写入对应曲线

    /* ========== 回包处理 ========== */
    void backfillConnectPayload(const QVariantList &values);          ///< 回填电机连接参数
    void applyZeroCalibrationPayload(const QVariantList &values);    ///< 应用零位校准结果

    /** @brief 获取当前主题调色板 */
    ThemePalette currentTheme() const;

    /* ========== 成员变量 ========== */

    // ---- 框架 ----
    Ui::Widget *ui;                      ///< Qt Designer UI 对象
    SerialManager *m_serial;             ///< 串口管理器

    // ---- 顶部栏 ----
    QPushButton *m_settingsBtn;          ///< 设置按钮

    // ---- 串口配置 ----
    QComboBox *m_portCombo;              ///< 串口号选择框
    QComboBox *m_baudCombo;              ///< 波特率选择框
    QPushButton *m_openCloseBtn;         ///< 打开/关闭串口按钮
    QLabel *m_statusLabel;               ///< 串口状态文本
    QPushButton *m_logBtn;               ///< 串口日志按钮
    QLabel *m_motorLamp;                 ///< 电机连接指示灯
    QLabel *m_serialLamp;                ///< 串口连接指示灯

    // ---- 电机配置 ----
    QPushButton *m_motorConnectBtn;      ///< 电机连接按钮
    QLineEdit *m_polePairsEdit;          ///< 极对数输入框
    QLineEdit *m_angleDirEdit;           ///< 角度方向输入框
    QLineEdit *m_speedDirEdit;           ///< 速度方向输入框
    QLabel *m_mosTempText;               ///< MOS 温度文本
    QSlider *m_mosTempSlider;            ///< MOS 温度滑条

    // ---- 零位校准 ----
    QLineEdit *m_zeroOffsetEdit;         ///< 零偏值输入框
    QLineEdit *m_elecAngleEdit;          ///< 电角度输入框
    QPushButton *m_zeroCalibBtn;         ///< 零电位校准按钮

    // ---- 波形控制 ----
    QComboBox *m_ctrlModeCombo;          ///< 控制模式选择（开环/电流/速度/位置）
    QHash<QPushButton *, int> m_trendOpenCmd;   ///< 波形按钮 → 开启命令映射
    QHash<QPushButton *, int> m_trendCloseCmd;  ///< 波形按钮 → 关闭命令映射
    QButtonGroup *m_trendGroup;          ///< 波形按钮互斥分组

    // ---- 命令输入映射 ----
    QHash<int, QLineEdit *> m_commandEdits;      ///< 命令字 → 输入框映射
    QHash<QString, QLineEdit *> m_pidEdits;      ///< PID 名称 → 输入框映射

    // ---- 图表 ----
    QDialog *m_logDialog;                ///< 日志对话框
    QTextEdit *m_logText;                ///< 日志文本框
    QCustomPlot *m_plotWidget;           ///< QCustomPlot 图表控件
    PlotManager *m_plotManager;          ///< 图表管理器
    QSlider *m_plotRangeSlider;          ///< 横轴范围滑条
    QPushButton *m_pauseBtn;             ///< 暂停/恢复曲线按钮

    // ---- 设置 ----
    QDialog *m_settingsDialog;           ///< 设置对话框
    QComboBox *m_themeCombo;             ///< 主题选择框

    // ---- 窗口拖动 ----
    QWidget *m_dragHandle;               ///< 拖动句柄（标题栏）
    bool m_dragging;                     ///< 是否正在拖动
    QPoint m_dragOffset;                 ///< 拖动偏移量

    // ---- 主题状态 ----
    int m_themeIndex;                    ///< 当前主题索引（0-5）
    int m_accentIndex;                   ///< 当前强调色索引（0-3）
    bool m_themeRebuildScheduled;        ///< 是否已安排界面重建
};

#endif // WIDGET_H
