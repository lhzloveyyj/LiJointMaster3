#include "widget.h"
#include "./ui_widget.h"
#include "plot/plotmanager.h"
#include "third_party/qcustomplot/qcustomplot.h"
#include "serial/serialcommand.h"
#include "serial/serialmanager.h"

/**
 * @file widget.cpp
 * @brief FOC 电机上位机主窗口实现
 *
 * 这是 LiJointMaster3 最大的源文件（约 1670 行），负责：
 *
 * 1. UI 构建：无边框窗口、6 主题 × 4 强调色切换、全动态布局
 * 2. 信号绑定：串口通信 ↔ 界面控件的双向数据绑定
 * 3. 协议路由：将串口回包解析结果分发到图表、参数回填、状态更新
 * 4. 交互保护：未开串口时阻止操作、波形互斥、自动断连状态复位
 *
 * 布局结构：
 * ┌──────────────────────────────────────────────┐
 * │              自定义标题栏                      │
 * ├──────────┬───────────────────────────────────┤
 * │ 左侧面板  │       图表区 (QCustomPlot)        │
 * │ 串口配置  │                                   │
 * │ 电机参数  ├───────────────────────────────────┤
 * │          │  底部调试台（零位/控制模式/PID等）  │
 * └──────────┴───────────────────────────────────┘
 *
 * 注意：主题切换时 buildUi() 会被重新调用，完全重建界面。
 */

#include <QComboBox>
#include <QColor>
#include <QDesktopServices>
#include <QDialog>
#include <QPainter>
#include <QPainterPath>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QObject>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QEvent>
#include <QScrollArea>
#include <QSlider>
#include <QStyleOptionComboBox>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

namespace {
// ============================================================================
// 匿名命名空间：辅助函数和工具类
// ============================================================================

/**
 * @brief 格式化数值，去掉无意义的末尾零和负零
 *
 * 例如：123.400000 → "123.4"，0.000000 → "0"
 */
QString formatCompactNumber(double value, int decimals = 6)
{
    QString s = QString::number(value, 'f', decimals);
    while (s.contains('.') && s.endsWith('0')) {
        s.chop(1);
    }
    if (s.endsWith('.')) {
        s.chop(1);
    }
    if (s == "-0") {
        s = "0";
    }
    return s;
}

/**
 * @brief 生成 QPushButton 的渐变样式表
 *
 * 根据基础颜色自动计算 normal/hover/pressed/checked 四种状态的颜色，
 * 每个状态使用轻微的亮度变化和渐变效果。
 *
 * @param bg     基础背景色（十六进制，如 "#3f72b8"）
 * @param text   文字颜色
 * @param border 边框颜色
 * @param radius 圆角半径
 * @return 完整的 QPushButton 样式表字符串
 */
QString makeButtonStyle(const QString &bg, const QString &text, const QString &border, int radius = 6)
{
    QColor base(bg);
    if (!base.isValid()) {
        base = QColor("#3f72b8");
    }
    const QString normalTop = base.lighter(118).name(QColor::HexRgb);
    const QString normalBottom = base.name(QColor::HexRgb);
    const QString hoverTop = base.lighter(128).name(QColor::HexRgb);
    const QString hoverBottom = base.lighter(108).name(QColor::HexRgb);
    const QString pressTop = base.darker(110).name(QColor::HexRgb);
    const QString pressBottom = base.darker(132).name(QColor::HexRgb);
    const QString checkedTop = base.lighter(125).name(QColor::HexRgb);
    const QString checkedBottom = base.lighter(106).name(QColor::HexRgb);
    const QString checkedHoverTop = base.lighter(136).name(QColor::HexRgb);
    const QString checkedHoverBottom = base.lighter(114).name(QColor::HexRgb);
    const QString checkedPressTop = base.darker(105).name(QColor::HexRgb);
    const QString checkedPressBottom = base.darker(125).name(QColor::HexRgb);
    return QString(
        "QPushButton{"
        "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1, stop:1 %2);"
        "color:%3;border:2px solid %4;border-radius:%5px;padding:3px 12px;font-weight:700;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %6, stop:1 %7);border-color:%6;}"
        "QPushButton:pressed{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %8, stop:1 %9);border-color:%9;padding-top:5px;padding-left:13px;padding-right:11px;padding-bottom:1px;}"
        "QPushButton:checked{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %10, stop:1 %11);border-color:%10;}"
        "QPushButton:checked:hover{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %12, stop:1 %13);border-color:%12;}"
        "QPushButton:checked:pressed{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %14, stop:1 %15);border-color:%15;padding-top:5px;padding-left:13px;padding-right:11px;padding-bottom:1px;}")
        .arg(normalTop, normalBottom, text, border)
        .arg(radius)
        .arg(hoverTop, hoverBottom, pressTop, pressBottom, checkedTop, checkedBottom, checkedHoverTop, checkedHoverBottom, checkedPressTop, checkedPressBottom);
}

/**
 * @brief 弹出"串口未打开"提示对话框
 *
 * 当用户尝试在串口未连接的情况下操作电机、波形或 PID 时，
 * 显示一个风格化的提示框，引导用户先打开串口。
 *
 * 包含自定义标题栏、渐变背景和圆角。
 */
void showSerialNotOpenTipDialog(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setModal(true);
    dlg.setFixedSize(360, 220);

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QFrame(&dlg);
    header->setFixedHeight(54);
    header->setStyleSheet(
        "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #5fa9ff, stop:1 #4d93ea);"
        "border-top-left-radius:12px;border-top-right-radius:12px;}");
    auto *headerL = new QHBoxLayout(header);
    headerL->setContentsMargins(14, 0, 14, 0);
    auto *title = new QLabel(QStringLiteral("i  小提示"), header);
    title->setStyleSheet("QLabel{color:white;font-size:14px;font-weight:700;}");
    headerL->addWidget(title);
    headerL->addStretch();

    auto *body = new QFrame(&dlg);
    body->setStyleSheet(
        "QFrame{background:#dfe4eb;border-bottom-left-radius:12px;border-bottom-right-radius:12px;}");
    auto *bodyL = new QVBoxLayout(body);
    bodyL->setContentsMargins(20, 18, 20, 14);
    bodyL->setSpacing(16);

    auto *msg = new QLabel(QStringLiteral("先打开串口，再连接电机呀~"), body);
    msg->setAlignment(Qt::AlignCenter);
    msg->setWordWrap(true);
    msg->setMinimumHeight(56);
    msg->setStyleSheet("QLabel{color:#1f3558;font-size:20px;font-weight:700;}");
    auto *ok = new QPushButton(QStringLiteral("知道啦"), body);
    ok->setFixedSize(110, 36);
    ok->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #52a9ff, stop:1 #3f8de6);"
        "color:white;border:none;border-radius:18px;font-size:16px;font-weight:700;}"
        "QPushButton:hover{background:#63b4ff;}"
        "QPushButton:pressed{background:#357bc8;}");

    bodyL->addWidget(msg);
    bodyL->addWidget(ok, 0, Qt::AlignHCenter);

    root->addWidget(header);
    root->addWidget(body, 1);

    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

/**
 * @brief 按钮悬停/按压动画过滤器
 *
 * 使用 QPropertyAnimation 对 QPushButton 的 geometry 属性做动画，
 * 实现悬停放大（1.02×）、按下缩小（0.90×）的"弹性"效果。
 *
 * 动画机制：
 * - Enter 事件：按钮放大到 1.02 倍
 * - Leave 事件：回复到 1.0 倍
 * - Pressed 事件：缩小到 0.90 倍
 * - Released 事件：根据鼠标位置恢复到 1.0 或 1.02 倍
 * - 所有动画使用 OutCubic 缓动曲线，90~120ms 持续时间
 */
class ButtonMotionFilter final : public QObject
{
public:
    explicit ButtonMotionFilter(QPushButton *btn)
        : QObject(btn)
        , m_btn(btn)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched != m_btn) {
            return QObject::eventFilter(watched, event);
        }
        if (event->type() == QEvent::Show) {
            m_btn->setProperty("_baseGeom", m_btn->geometry());
        } else if (event->type() == QEvent::Enter) {
            if (!m_btn->isDown()) {
                animateToScale(1.02, 90);
            }
        } else if (event->type() == QEvent::Leave) {
            if (!m_btn->isDown()) {
                animateToScale(1.0, 90);
            }
        }
        return QObject::eventFilter(watched, event);
    }

    /**
     * @brief 动画缩放到指定比例
     *
     * 以按钮中心为基准，计算缩放后的目标矩形。
     * 动画完成后自动清理。
     *
     * @param scale       缩放比例（1.0 = 原始大小）
     * @param durationMs  动画时长（毫秒）
     */
    void animateToScale(double scale, int durationMs)
    {
        QRect base = m_btn->property("_baseGeom").toRect();
        if (base.isNull()) {
            base = m_btn->geometry();
            m_btn->setProperty("_baseGeom", base);
        }
        const int dw = qRound(base.width() * (1.0 - scale) * 0.5);
        const int dh = qRound(base.height() * (1.0 - scale) * 0.5);
        const QRect target = base.adjusted(dw, dh, -dw, -dh);

        auto *anim = new QPropertyAnimation(m_btn, "geometry");
        anim->setDuration(durationMs);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(m_btn->geometry());
        anim->setEndValue(target);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

private:
    QPushButton *m_btn;  ///< 被观察的按钮对象
};

/**
 * @brief 自定义下拉框，使用三角形箭头替代系统默认箭头
 *
 * 重写 paintEvent 在右侧绘制一个自定义三角形箭头，
 * 避免系统主题在不同平台上渲染不一致的问题。
 */
class ChevronComboBox final : public QComboBox
{
public:
    explicit ChevronComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QComboBox::paintEvent(event);
        Q_UNUSED(event);

        // 获取下拉框的箭头区域
        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QRect arrowRect = style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, this);
        if (!arrowRect.isValid()) {
            return;
        }

        // 在箭头区域绘制一个倒三角形（▼）
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Text));

        const QPointF center = arrowRect.center();
        QPainterPath path;
        path.moveTo(center.x() - 4.0, center.y() - 1.5);
        path.lineTo(center.x() + 4.0, center.y() - 1.5);
        path.lineTo(center.x(), center.y() + 3.5);
        path.closeSubpath();
        painter.drawPath(path);
    }
};

/**
 * @brief 为按钮安装弹跳动画效果
 *
 * 安装 ButtonMotionFilter 事件过滤器，并连接 pressed/released/toggled 信号，
 * 实现按下缩小、松开弹回的动画效果。
 */
void installButtonBounce(QPushButton *btn)
{
    auto *filter = new ButtonMotionFilter(btn);
    btn->installEventFilter(filter);
    btn->setProperty("_baseGeom", btn->geometry());

    QObject::connect(btn, &QPushButton::pressed, btn, [filter]() {
        filter->animateToScale(0.90, 120);  // 按下缩小到 90%
    });
    QObject::connect(btn, &QPushButton::released, btn, [btn, filter]() {
        filter->animateToScale(btn->underMouse() ? 1.02 : 1.0, 120);  // 松手弹回
    });
    QObject::connect(btn, &QPushButton::toggled, btn, [btn, filter](bool) {
        filter->animateToScale(btn->underMouse() ? 1.02 : 1.0, 90);
    });
}

/**
 * @brief 创建一个带渐变背景和圆角的卡片容器
 *
 * @param bg     背景色基础色
 * @param border 边框颜色
 * @param radius 圆角半径
 * @return 样式化的 QFrame 卡片
 */
QFrame *makeCard(const QString &bg, const QString &border, int radius = 10)
{
    auto *card = new QFrame;
    card->setFrameShape(QFrame::NoFrame);
    QColor bgColor(bg);
    QColor borderColor(border);
    if (!bgColor.isValid()) {
        bgColor = QColor("#1c273b");
    }
    if (!borderColor.isValid()) {
        borderColor = QColor("#4b5f83");
    }
    card->setStyleSheet(QString(
        "QFrame{"
        "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1, stop:1 %2);"
        "border:2px solid %3;border-radius:%4px;}")
        .arg(bgColor.lighter(110).name(QColor::HexRgb), bgColor.name(QColor::HexRgb), borderColor.lighter(108).name(QColor::HexRgb))
        .arg(radius));
    return card;
}

/**
 * @brief 创建一个通用操作按钮（带弹跳动画）
 *
 * @param text 按钮文字
 * @param bg   基础背景色
 * @param h    最小高度
 * @return 样式化的 QPushButton
 */
QPushButton *makeBtn(const QString &text, const QString &bg, int h = 30)
{
    auto *btn = new QPushButton(text);
    btn->setMinimumHeight(h);
    btn->setStyleSheet(makeButtonStyle(bg, "#eaf2ff", "#5a6b88", 6));
    installButtonBounce(btn);
    return btn;
}

/**
 * @brief 创建标题栏小按钮（关闭/最小化用）
 *
 * 按钮尺寸固定为 26×26，带圆形悬停高亮效果。
 *
 * @param text    按钮文字（如 "×"）
 * @param bg      正常背景色
 * @param hoverBg 悬停背景色
 * @return 样式化的 QPushButton
 */
QPushButton *makeTitleBarBtn(const QString &text, const QString &bg, const QString &hoverBg)
{
    auto *btn = new QPushButton(text);
    btn->setFixedSize(26, 26);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString(
        "QPushButton{background:%1;color:#ffffff;border:1px solid rgba(255,255,255,0.18);border-radius:8px;font-size:14px;font-weight:700;padding:0;}"
        "QPushButton:hover{background:%2;border:1px solid rgba(255,255,255,0.28);}"
        "QPushButton:pressed{background:%3;padding-top:1px;}")
        .arg(bg, hoverBg, QColor(hoverBg).darker(118).name(QColor::HexRgb)));
    return btn;
}

/**
 * @brief 创建一个输入框
 *
 * @param ph 占位提示文本
 * @return QLineEdit 指针
 */
QLineEdit *makeInput(const QString &ph)
{
    auto *e = new QLineEdit;
    e->setPlaceholderText(ph);
    e->setMinimumHeight(27);
    return e;
}

/**
 * @brief 创建一个自定义下拉框（带三角形箭头）
 *
 * @return ChevronComboBox 指针
 */
QComboBox *makeCombo()
{
    auto *c = new ChevronComboBox;
    c->setMinimumHeight(27);
    return c;
}
}

// ============================================================================
// Widget 构造函数
// ============================================================================

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , m_serial(new SerialManager(this))     // 创建串口管理器（自动开始 300ms 轮询）
    , m_settingsBtn(nullptr)
    , m_portCombo(nullptr)
    , m_baudCombo(nullptr)
    , m_openCloseBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_logBtn(nullptr)
    , m_motorLamp(nullptr)
    , m_serialLamp(nullptr)
    , m_motorConnectBtn(nullptr)
    , m_polePairsEdit(nullptr)
    , m_angleDirEdit(nullptr)
    , m_speedDirEdit(nullptr)
    , m_mosTempText(nullptr)
    , m_mosTempSlider(nullptr)
    , m_zeroOffsetEdit(nullptr)
    , m_elecAngleEdit(nullptr)
    , m_verifyLabels{nullptr}
    , m_zeroCalibBtn(nullptr)
    , m_ctrlModeCombo(nullptr)
    , m_trendGroup(nullptr)
    , m_logDialog(nullptr)
    , m_logText(nullptr)
    , m_plotWidget(nullptr)
    , m_plotManager(nullptr)
    , m_plotRangeSlider(nullptr)
    , m_pauseBtn(nullptr)
    , m_settingsDialog(nullptr)
    , m_themeCombo(nullptr)
    , m_dragHandle(nullptr)
    , m_dragging(false)
    , m_dragOffset()
    , m_themeIndex(0)          // 默认蓝色主题
    , m_accentIndex(0)         // 默认蓝色强调色
    , m_themeRebuildScheduled(false)
{
    // 无边框窗口
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    ui->setupUi(this);
    buildUi();  // 构建完整 UI
}

Widget::~Widget()
{
    delete ui;
}

// ============================================================================
// 事件过滤器 —— 实现无边框窗口拖动
// ============================================================================

/**
 * @brief 事件过滤器：实现标题栏拖拽窗口
 *
 * 捕获 m_dragHandle（标题栏）上的 MouseButtonPress / MouseMove / MouseButtonRelease
 * 事件，计算拖拽偏移量并调用 move() 实现无边框窗口拖动。
 *
 * 在 Wayland 下优先使用 startSystemMove() 实现原生窗口拖动。
 */
bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    // 只处理 m_dragHandle 及其子控件上的事件
    const bool isDragTarget = watched == m_dragHandle
        || (m_dragHandle && watched && watched->parent() == m_dragHandle);
    if (!isDragTarget) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }
        m_dragging = true;
        m_dragOffset = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
        if (windowHandle() && windowHandle()->startSystemMove()) {
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (m_dragging && (mouseEvent->buttons() & Qt::LeftButton)) {
            move(mouseEvent->globalPosition().toPoint() - m_dragOffset);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease:
        m_dragging = false;
        break;
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

// ============================================================================
// 图表曲线注册 —— 注册所有 FOC 调试曲线到 PlotManager
// ============================================================================

/**
 * @brief 注册所有图表曲线到 PlotManager
 *
 * 有感模式：
 *   mechanicalAngle, Ua/Ub/Uc, ADC1/2/3, Ta/Tb/Tc, Ia/Ib/Ic,
 *   Ualpha/Ubeta, Ialpha/Ibeta, Iq/Id, speed/speedOut,
 *   local/localOut, adcvbus, electricalAngle
 *
 * 曲线名称必须与 appendTrendValues() 中引用的一致。
 */
void Widget::setupPlotGraphs()
{
    if (!m_plotManager) {
        return;
    }

    // 有感 FOC 调试曲线：这些名字必须和 appendTrendValues() 里的写入名字一致。
    // PlotManager 会按名字找到对应曲线，因此新增串口数据流时要同时在这里注册曲线。
    m_plotManager->addGraph("mechanicalAngle", Qt::red);
    m_plotManager->addGraph("Ua", Qt::red);
    m_plotManager->addGraph("Ub", Qt::green);
    m_plotManager->addGraph("Uc", Qt::blue);
    m_plotManager->addGraph("ADC1", Qt::yellow);
    m_plotManager->addGraph("ADC2", Qt::cyan);
    m_plotManager->addGraph("ADC3", Qt::magenta);
    m_plotManager->addGraph("Ta", Qt::yellow);
    m_plotManager->addGraph("Tb", Qt::cyan);
    m_plotManager->addGraph("Tc", Qt::magenta);
    m_plotManager->addGraph("Ia", Qt::yellow);
    m_plotManager->addGraph("Ib", Qt::cyan);
    m_plotManager->addGraph("Ic", Qt::magenta);
    m_plotManager->addGraph("Ualpha", Qt::yellow);
    m_plotManager->addGraph("Ubeta", Qt::cyan);
    m_plotManager->addGraph("Ialpha", Qt::yellow);
    m_plotManager->addGraph("Ibeta", Qt::cyan);
    m_plotManager->addGraph("Iq", Qt::yellow);
    m_plotManager->addGraph("Id", Qt::cyan);
    m_plotManager->addGraph("speed", Qt::yellow);
    m_plotManager->addGraph("speedOut", Qt::magenta);
    m_plotManager->addGraph("local", Qt::blue);
    m_plotManager->addGraph("localOut", Qt::yellow);
    m_plotManager->addGraph("adcvbus", Qt::green);
    m_plotManager->addGraph("correctedAngle", QColor(255, 128, 0));

}

// ============================================================================
// 趋势数据分发 —— 将串口回包数据写入对应图表曲线
// ============================================================================

/**
 * @brief 根据串口命令字，将回包数据分发到对应的图表曲线
 *
 * 命令字与曲线的映射关系：
 * - 单值命令（机械角度、速度、位置等）→ 一条曲线
 * - 双值命令（Uα/Uβ、Iα/Iβ、Iq/Id 等）→ 两条曲线
 * - 三值命令（三相电压、电流、ADC 等）→ 三条曲线
 *
 * @param command 串口命令字
 * @param values  解析出的 float 值列表
 */
void Widget::appendTrendValues(int command, const QVariantList &values)
{
    if (!m_plotManager || values.isEmpty()) {
        return;
    }

    const auto valueAt = [&values](int index) -> double {
        return index < values.size() ? values[index].toDouble() : 0.0;
    };

    switch (static_cast<SerialCommand>(command)) {
    case SerialCommand::CMD_MECHANICALANGLE:
        if (values.size() >= 2) {
            m_plotManager->appendData("mechanicalAngle", valueAt(0));
            m_plotManager->appendData("correctedAngle", valueAt(1));
        } else {
            m_plotManager->appendData("mechanicalAngle", valueAt(0));
        }
        break;
    case SerialCommand::CMD_UABC:
        if (values.size() >= 3) {
            m_plotManager->appendData("Ua", valueAt(0));
            m_plotManager->appendData("Ub", valueAt(1));
            m_plotManager->appendData("Uc", valueAt(2));
        }
        break;
    case SerialCommand::CMD_ADC:
        if (values.size() >= 3) {
            m_plotManager->appendData("ADC1", valueAt(0));
            m_plotManager->appendData("ADC2", valueAt(1));
            m_plotManager->appendData("ADC3", valueAt(2));
        }
        break;
    case SerialCommand::CMD_TABC:
        if (values.size() >= 3) {
            m_plotManager->appendData("Ta", valueAt(0));
            m_plotManager->appendData("Tb", valueAt(1));
            m_plotManager->appendData("Tc", valueAt(2));
        }
        break;
    case SerialCommand::CMD_IABC:
        if (values.size() >= 3) {
            m_plotManager->appendData("Ia", valueAt(0));
            m_plotManager->appendData("Ib", valueAt(1));
            m_plotManager->appendData("Ic", valueAt(2));
        }
        break;
    case SerialCommand::CMD_UALPHA_BETA:
        if (values.size() >= 2) {
            m_plotManager->appendData("Ualpha", valueAt(0));
            m_plotManager->appendData("Ubeta", valueAt(1));
        }
        break;
    case SerialCommand::CMD_IALPHA_BETA:
        if (values.size() >= 2) {
            m_plotManager->appendData("Ialpha", valueAt(0));
            m_plotManager->appendData("Ibeta", valueAt(1));
        }
        break;
    case SerialCommand::CMD_IQ_ID:
        if (values.size() >= 2) {
            m_plotManager->appendData("Iq", valueAt(0));
            m_plotManager->appendData("Id", valueAt(1));
        }
        break;
    case SerialCommand::CMD_SPEED:
        m_plotManager->appendData("speed", valueAt(0));
        break;
    case SerialCommand::CMD_SPEEDOUT:
        m_plotManager->appendData("speedOut", valueAt(0));
        break;
    case SerialCommand::CMD_LOCAL:
        m_plotManager->appendData("local", valueAt(0));
        break;
    case SerialCommand::CMD_LOCALOUT:
        m_plotManager->appendData("localOut", valueAt(0));
        break;
    case SerialCommand::CMD_ADCVBUS:
        if (m_serial && m_serial->property("activeTrendCommand").toInt() == static_cast<int>(SerialCommand::CMD_ADCVBUS)) {
            m_plotManager->appendData("adcvbus", valueAt(0));
        }
        break;
    case SerialCommand::CMD_ELECTRICALANGLE:
        if (values.size() >= 2) {
            m_plotManager->appendData("correctedAngle", valueAt(0));
            m_plotManager->appendData("mechanicalAngle", valueAt(1));
        } else {
            m_plotManager->appendData("electricalAngle", valueAt(0));
        }
        break;
    case SerialCommand::CMD_TELEMETRY_BUNDLE: {
        /* values[0..]: int16 数组，前2个是 mask 低16位+高16位 */
        if (values.size() < 3) break;
        uint32_t mask = ((uint32_t)(int16_t)values[0].toInt())
                      | ((uint32_t)(int16_t)values[1].toInt() << 16);
        int vi = 2;
        auto r16 = [&]() -> double { return vi < values.size() ? (int16_t)values[vi++].toInt() : 0; };
        if (mask & 1)    { m_plotManager->appendData("mechanicalAngle", r16()/1000.0); }
        if (mask & 2)    { m_plotManager->appendData("speed", r16()/10.0); }
        if (mask & 4)    { m_plotManager->appendData("speedOut", r16()/100.0); }
        if (mask & 8)    { m_plotManager->appendData("Ia", r16()/100.0); m_plotManager->appendData("Ib", r16()/100.0); m_plotManager->appendData("Ic", r16()/100.0); }
        if (mask & 16)   { m_plotManager->appendData("Iq", r16()/100.0); m_plotManager->appendData("Id", r16()/100.0); }
        if (mask & 32)   { m_plotManager->appendData("Ualpha", r16()/100.0); m_plotManager->appendData("Ubeta", r16()/100.0); }
        if (mask & 64)   { m_plotManager->appendData("Ua", r16()/100.0); m_plotManager->appendData("Ub", r16()/100.0); m_plotManager->appendData("Uc", r16()/100.0); }
        if (mask & 128)  { m_plotManager->appendData("ADC1", r16()); m_plotManager->appendData("ADC2", r16()); m_plotManager->appendData("ADC3", r16()); }
        if (mask & 256)  { m_plotManager->appendData("Ta", r16()/10000.0); m_plotManager->appendData("Tb", r16()/10000.0); m_plotManager->appendData("Tc", r16()/10000.0); }
        if (mask & 512)  { m_plotManager->appendData("adcvbus", r16()/100.0); }
        if (mask & 1024) { m_plotManager->appendData("local", r16()/1000.0); }
        if (mask & 2048) { m_plotManager->appendData("localOut", r16()/10.0); }
        if (mask & 4096) { m_plotManager->appendData("Ialpha", r16()/100.0); m_plotManager->appendData("Ibeta", r16()/100.0); }
        if (mask & 8192) { m_plotManager->appendData("correctedAngle", r16()/1000.0); }
        break;
    }
    default:
        break;
    }
}

// ============================================================================
// buildUi —— 核心 UI 构建函数
// ============================================================================

/**
 * @brief 构建（或重建）完整用户界面
 *
 * 本函数在以下情况被调用：
 * 1. Widget 构造时
 * 2. 用户切换主题或强调色时
 *
 * 重建流程：
 * 1. 断开旧串口信号连接，避免重复绑定
 * 2. 清空命令映射表和 UI 指针
 * 3. 根据当前主题索引加载配色方案并生成全局样式表
 * 4. 清空旧布局中的所有子控件
 * 5. 重建：标题栏 → 左侧面板（串口+电机）→ 右侧面板（图表+调试台）
 * 6. 绑定额外的信号/槽
 * 7. 刷新串口列表
 *
 * 注意：主题切换时界面完全重建，所有 UI 指针需要重新赋值。
 *       旧控件通过递归删除布局项来自动析构。
 */
void Widget::buildUi()
{
    if (m_serial) {
        disconnect(m_serial, nullptr, this, nullptr);
    }
    m_commandEdits.clear();
    m_trendOpenCmd.clear();
    m_trendCloseCmd.clear();
    m_plotWidget = nullptr;
    m_plotManager = nullptr;
    m_plotRangeSlider = nullptr;

    // ---- 主题配色定义 ----
    // 6 套主题轮换：蓝色(默认)、浅色、纯黑、绿色、暖色、紫色
    // 每套主题包含窗口背景、面板背景、卡片背景、边框、主文字、副文字色
    struct Theme {
        QString windowBg;
        QString panelBg;
        QString cardBg;
        QString border;
        QString text;
        QString subText;
    };

    const QList<Theme> themes = {
        {"#0d1423", "#15213a", "#223150", "#6d88bb", "#f4f8ff", "#cbd8f6"},  // 蓝色（默认）
        {"#eaf3ff", "#f8fbff", "#ffffff", "#9db7da", "#2a4268", "#6780a8"},  // 浅色
        {"#0b0b0b", "#121212", "#1a1a1a", "#4a4a4a", "#f5f5f5", "#c8c8c8"},  // 纯黑
        {"#112018", "#193126", "#254535", "#6fa98b", "#eefdf4", "#b8ddc7"},  // 绿色
        {"#241712", "#35211a", "#4a2d24", "#c58763", "#fff4ee", "#e8bfaa"},  // 暖色
        {"#1c1730", "#282046", "#382c61", "#9787df", "#faf6ff", "#d7cbf7"}   // 紫色
    };
    // 4 种强调色：蓝色、绿色、橙色、红色
    const QList<QString> accents = {"#5d93ff", "#59d98d", "#ffbf69", "#ff7f96"};

    const Theme th = themes[qBound(0, m_themeIndex, themes.size() - 1)];
    const QString accentColor = accents[qBound(0, m_accentIndex, accents.size() - 1)];
    const QString accentBrush = accentColor;
    const QColor accent(accentBrush);
    const QString panelHi = QColor(th.panelBg).lighter(112).name(QColor::HexRgb);
    const QString cardHi = QColor(th.cardBg).lighter(116).name(QColor::HexRgb);
    const QString borderHi = QColor(th.border).lighter(118).name(QColor::HexRgb);

    setWindowTitle(QStringLiteral("LiJoint FOC 上位机"));
    setFixedSize(1480, 920);

    const QString btnBase = th.cardBg;
    const QString btnHover = accentBrush;
    setStyleSheet(
        QString("QWidget{font-family:'Microsoft YaHei UI';font-size:12px;color:%1;background:%2;selection-background-color:%5;}"
        "QLabel{color:%1;}"
        "QGroupBox{border:2px solid %3;border-radius:16px;margin-top:10px;padding-top:10px;background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %6, stop:1 %4);}"
        "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:2px 10px;color:%1;font-weight:700;background:%6;border:2px solid %3;border-radius:10px;}"
        "QPushButton{background:%7;color:%1;border:2px solid %3;border-radius:12px;padding:3px 10px;font-weight:700;}"
        "QPushButton:hover{background:%8;}"
        "QPushButton:pressed{background:%8;}"
        "QPushButton:checked{background:%8;border-color:%8;}"
        "QLineEdit,QComboBox{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %6, stop:1 %4);color:%1;border:2px solid %3;border-radius:12px;min-height:28px;}"
        "QLineEdit{padding:0 8px 0 10px;}"
        "QComboBox{padding:0 28px 0 10px;}"
        "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:24px;border:none;}"
        "QComboBox::down-arrow{image:none;width:0px;height:0px;}"
        "QComboBox QAbstractItemView{background:%4;color:%1;border:2px solid %3;border-radius:10px;selection-background-color:%5;selection-color:%1;}"
        "QSlider::groove:horizontal{height:10px;background:%3;border-radius:5px;border:1px solid %6;}"
        "QSlider::sub-page:horizontal{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %5, stop:1 %8);border-radius:5px;}"
        "QSlider::handle:horizontal{width:18px;margin:-6px 0;border-radius:9px;background:%1;border:3px solid %5;}"
        "QTextEdit{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %6, stop:1 %4);border:2px solid %3;border-radius:14px;padding:8px;}")
        .arg(th.text, th.windowBg, th.border, th.cardBg, accentColor, cardHi, btnBase, btnHover)
    );

    // 主题切换时会重复调用 buildUi，先清空旧布局和旧控件，避免新旧界面叠加
    if (auto *oldLayout = layout()) {
        const auto clearLayout = [&](auto &&self, QLayout *l) -> void {
            if (!l) {
                return;
            }
            QLayoutItem *item = nullptr;
            while ((item = l->takeAt(0)) != nullptr) {
                if (auto *w = item->widget()) {
                    delete w;
                }
                if (auto *childLayout = item->layout()) {
                    self(self, childLayout);
                }
                delete item;
            }
        };
        clearLayout(clearLayout, oldLayout);
        delete oldLayout;
    }

    auto *mainV = new QVBoxLayout(this);
    mainV->setContentsMargins(4, 4, 4, 4);
    mainV->setSpacing(4);

    // Top bar (match screenshot-2 style)
    auto *topBar = makeCard(th.panelBg, th.border, 12);
    topBar->setFixedHeight(54);
    m_dragHandle = topBar;
    topBar->installEventFilter(this);
    auto *topL = new QHBoxLayout(topBar);
    topL->setContentsMargins(10, 6, 10, 6);
    topL->setSpacing(8);

    auto *mark = new QLabel;
    mark->setFixedSize(14, 26);
    mark->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1, stop:1 %2);border:2px solid %3;border-radius:7px;")
                            .arg(accent.lighter(125).name(QColor::HexRgb), accent.name(QColor::HexRgb), borderHi));
    mark->installEventFilter(this);
    auto *title = new QLabel(QStringLiteral("LiJointMaster"));
    title->setStyleSheet(QString("font-size:24px;font-weight:900;color:%1;letter-spacing:1px;").arg(th.text));
    title->installEventFilter(this);

    auto *titleTag = new QLabel(QStringLiteral("cartoon"));
    titleTag->setStyleSheet(QString("QLabel{background:%1;color:%2;border:2px solid %3;border-radius:10px;padding:2px 10px;font-size:11px;font-weight:800;}")
                                .arg(accent.lighter(120).name(QColor::HexRgb), th.windowBg, borderHi));
    titleTag->installEventFilter(this);

    auto *settingsBtn = makeBtn(QStringLiteral("设置"), accentBrush, 28);
    settingsBtn->setFixedWidth(60);
    m_settingsBtn = settingsBtn;
    auto *minimizeBtn = makeTitleBarBtn(QStringLiteral("—"), "rgba(255,255,255,0.10)", "#5d93ff");
    auto *closeBtn = makeTitleBarBtn(QStringLiteral("×"), "rgba(255,255,255,0.10)", "#ff6f84");

    topL->addWidget(mark);
    topL->addWidget(title);
    topL->addWidget(titleTag);
    topL->addSpacing(10);
    topL->addWidget(settingsBtn);
    topL->addStretch();
    topL->addWidget(minimizeBtn);
    topL->addWidget(closeBtn);

    mainV->addWidget(topBar);

    // Main split
    auto *body = new QHBoxLayout;
    body->setSpacing(6);

    // Left panel
    auto *left = makeCard(th.panelBg, th.border, 12);
    left->setFixedWidth(226);
    auto *leftV = new QVBoxLayout(left);
    leftV->setContentsMargins(6, 6, 6, 6);
    leftV->setSpacing(6);

    auto *serialCard = new QGroupBox(QStringLiteral("串口配置"));
    auto *sg = new QGridLayout(serialCard);
    sg->setContentsMargins(8, 10, 8, 8);
    sg->setHorizontalSpacing(4);
    sg->setVerticalSpacing(4);

    auto addRow = [&](int r, const QString &n, QWidget *w) {
        auto *l = new QLabel(n);
        l->setMinimumWidth(42);
        l->setStyleSheet(QString("color:%1;").arg(th.subText));
        sg->addWidget(l, r, 0);
        sg->addWidget(w, r, 1);
    };

    auto *com = makeCombo();
    auto *baud = makeCombo(); baud->addItems({"4000000", "2000000", "921600", "115200"});
    auto *db = makeCombo(); db->addItems({"8", "7"});
    auto *sb = makeCombo(); sb->addItems({"1", "1.5", "2"});
    auto *par = makeCombo(); par->addItems({"None", "Even", "Odd"});
    com->setMinimumWidth(98);
    baud->setMinimumWidth(98);
    db->setMinimumWidth(98);
    sb->setMinimumWidth(98);
    par->setMinimumWidth(98);

    addRow(0, QStringLiteral("串口"), com);
    addRow(1, QStringLiteral("波特率"), baud);
    addRow(2, QStringLiteral("数据位"), db);
    addRow(3, QStringLiteral("停止位"), sb);
    addRow(4, QStringLiteral("校验位"), par);

    auto *openSerialBtn = makeBtn(QStringLiteral("打开串口"), accentBrush, 28);
    auto *rowBtn1 = new QHBoxLayout;
    rowBtn1->addWidget(openSerialBtn);
    auto *lamp = new QLabel;
    lamp->setFixedSize(18, 18);
    lamp->setStyleSheet("border-radius:9px;background:#9da7b6;border:1px solid #d7deea;");
    rowBtn1->addWidget(lamp);
    rowBtn1->addStretch();
    sg->addLayout(rowBtn1, 5, 0, 1, 2);

    auto *logBtn = makeBtn(QStringLiteral("串口日志"), accentBrush, 28);
    auto *rowBtn2 = new QHBoxLayout;
    rowBtn2->addWidget(logBtn);
    rowBtn2->addStretch();
    sg->addLayout(rowBtn2, 6, 0, 1, 2);

    auto *status = new QLabel(QStringLiteral("状态：已刷新串口列表"));
    status->setStyleSheet(QString(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %1, stop:1 %2);"
        "border:2px solid %3;border-radius:12px;padding:6px 10px;color:%4;font-weight:700;")
                              .arg(cardHi, th.cardBg, borderHi, th.text));

    auto *motorCard = new QGroupBox(QStringLiteral("电机配置"));
    auto *mv = new QVBoxLayout(motorCard);
    mv->setSpacing(6);
    mv->setContentsMargins(10, 8, 10, 8);

    auto *motorConnectBtn = makeBtn(QStringLiteral("连接"), accentBrush, 28);
    auto *mTop = new QHBoxLayout;
    mTop->addWidget(motorConnectBtn);
    auto *mlamp = new QLabel;
    mlamp->setFixedSize(18, 18);
    mlamp->setStyleSheet("border-radius:9px;background:#9da7b6;border:1px solid #d7deea;");
    mTop->addWidget(mlamp);
    mTop->addStretch();
    mv->addLayout(mTop);

    // 这三项必须满足：按钮宽 + 间隔 + 输入框宽 <= 当前行可用宽度
    // 否则布局会挤压，视觉上看起来像“没有空隙”。
    const int kMotorBtnW = 99;
    const int kMotorEditW = 79;
    const int kMotorGap = 7;
    auto showSerialNotOpenTip = [this]() {
        if (m_serial) {
            m_serial->playSystemAlert();
        }
        if (m_serial) {
            m_serial->playSystemAlert();
        }
        showSerialNotOpenTipDialog(this);
    };
    auto motorRow = [&](const QString &t, int command, const QString &defaultValue = QStringLiteral("0")) {
        auto *r = new QHBoxLayout;
        r->setContentsMargins(2, 0, 2, 0);
        r->setSpacing(0);
        auto *btn = makeBtn(t, accentBrush, 28);
        auto *edit = makeInput(defaultValue);
        auto *gap = new QWidget;
        gap->setFixedWidth(kMotorGap);
        gap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        btn->setFixedWidth(kMotorBtnW);
        edit->setFixedWidth(kMotorEditW);
        r->addWidget(btn);
        r->addWidget(gap);
        r->addWidget(edit);
        r->addStretch();
        mv->addLayout(r);
        m_commandEdits.insert(command, edit);
        connect(btn, &QPushButton::clicked, this, [this, command, edit]() {
            if (!m_serial || !m_serial->isConnected()) {
                if (m_serial) {
                    m_serial->playSystemAlert();
                }
                showSerialNotOpenTipDialog(this);
                return;
            }
            const double value = QLocale().toDouble(edit->text());
            m_serial->sendFloatCommand(command, value);
        });
    };
    motorRow(QStringLiteral("设置极对数"), static_cast<int>(SerialCommand::CMD_SETPAIRS));
    motorRow(QStringLiteral("设置角度方向"), static_cast<int>(SerialCommand::CMD_SETDIR));
    motorRow(QStringLiteral("设置速度方向"), static_cast<int>(SerialCommand::CMD_SETSPEEDDIR));
    motorRow(QStringLiteral("设置Rs"), static_cast<int>(SerialCommand::CMD_SETMOTORRS), QStringLiteral("0.198"));
    motorRow(QStringLiteral("设置Lq"), static_cast<int>(SerialCommand::CMD_SETMOTORLQ), QStringLiteral("0.000074"));
    motorRow(QStringLiteral("设置Ld"), static_cast<int>(SerialCommand::CMD_SETMOTORLD), QStringLiteral("0.000040"));

    auto *mosTitle = new QHBoxLayout;
    mosTitle->setContentsMargins(0, 2, 0, 0);
    mosTitle->addWidget(new QLabel(QStringLiteral("MOS温度")));
    mosTitle->addStretch();
    auto *mosVal = new QLabel("0.0 ℃");
    mosVal->setStyleSheet("color:#3ee58c;font-weight:700;");
    mosTitle->addWidget(mosVal);
    mv->addLayout(mosTitle);

    auto *mos = new QSlider(Qt::Horizontal);
    mos->setRange(0, 120);
    mos->setFixedHeight(18);
    mos->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mv->addWidget(mos);

    // 余量放到最底部，保证 MOS 区贴近“母线电压”
    mv->addStretch(1);

    leftV->addWidget(serialCard);
    leftV->addWidget(status);
    leftV->addWidget(motorCard, 1);

    // Right panel
    auto *right = makeCard(th.panelBg, th.border, 12);
    auto *rightV = new QVBoxLayout(right);
    rightV->setContentsMargins(8, 8, 8, 8);
    rightV->setSpacing(8);

    auto *chartCard = makeCard(th.cardBg, th.border, 10);
    chartCard->setMinimumHeight(510);
    auto *chartV = new QVBoxLayout(chartCard);
    chartV->setContentsMargins(12, 12, 12, 12);

    auto *plotFrame = new QFrame(chartCard);
    plotFrame->setStyleSheet(QString(
        "QFrame{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #273654, stop:1 #1d2942);"
        "border:2px solid %1;border-radius:14px;}").arg(borderHi));
    auto *plotFrameL = new QVBoxLayout(plotFrame);
    plotFrameL->setContentsMargins(6, 6, 6, 6);
    m_plotWidget = new QCustomPlot(plotFrame);
    m_plotWidget->setMinimumHeight(420);
    m_plotWidget->setBackground(QBrush(QColor(30, 30, 30)));
    plotFrameL->addWidget(m_plotWidget);

    m_plotManager = new PlotManager(m_plotWidget, chartCard);
    setupPlotGraphs();

    auto *rangeRow = new QHBoxLayout;
    rangeRow->setSpacing(8);
    auto *rangeLabel = new QLabel(QStringLiteral("横轴范围"));
    rangeLabel->setStyleSheet(QString("color:%1;font-size:12px;").arg(th.subText));
    auto *plotRangeSlider = new QSlider(Qt::Horizontal);
    plotRangeSlider->setRange(0, 100);
    plotRangeSlider->setValue(49);
    auto *rangeHint = new QLabel(QStringLiteral("0.1s - 10s"));
    rangeHint->setStyleSheet(QString("color:%1;font-size:12px;").arg(th.subText));
    rangeRow->addWidget(rangeLabel);
    rangeRow->addWidget(plotRangeSlider, 1);
    rangeRow->addWidget(rangeHint);

    m_pauseBtn = makeBtn(QStringLiteral("暂停"), accentBrush, 28);
    m_pauseBtn->setCheckable(true);
    m_pauseBtn->setFixedWidth(64);
    connect(m_pauseBtn, &QPushButton::toggled, [this](bool checked) {
        m_pauseBtn->setText(checked ? QStringLiteral("继续") : QStringLiteral("暂停"));
        if (m_plotManager) m_plotManager->setPaused(checked);
    });
    rangeRow->addWidget(m_pauseBtn);

    chartV->addWidget(plotFrame, 1);
    chartV->addLayout(rangeRow);

    auto *bottomCard = makeCard(th.cardBg, th.border, 10);
    auto *bottomL = new QHBoxLayout(bottomCard);
    bottomL->setContentsMargins(4, 6, 4, 6);
    bottomL->setSpacing(3);

    auto *reserve = new QGroupBox(QStringLiteral("预留参数框"));
    reserve->setFixedWidth(166);
    reserve->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto *rv = new QVBoxLayout(reserve);
    rv->setContentsMargins(8, 8, 8, 8);
    rv->setSpacing(6);
    auto *calibRow = new QHBoxLayout;
    auto *zeroCalibBtn = makeBtn(QStringLiteral("校准"), accentBrush, 28);
    zeroCalibBtn->setCheckable(true);
    zeroCalibBtn->setFixedWidth(60);
    auto *verifyBtn = makeBtn(QStringLiteral("验证"), "#495b78", 28);
    verifyBtn->setFixedWidth(60);
    calibRow->addWidget(zeroCalibBtn);
    calibRow->addWidget(verifyBtn);
    calibRow->addStretch();
    rv->addLayout(calibRow);
    auto *zeroOffsetEdit = makeInput("0.000");
    auto *elecAngleEdit = makeInput("0.000");
    auto *zr = new QHBoxLayout; zr->addWidget(new QLabel(QStringLiteral("零偏值"))); zr->addWidget(zeroOffsetEdit);
    auto *er = new QHBoxLayout; er->addWidget(new QLabel(QStringLiteral("电角度"))); er->addWidget(elecAngleEdit);
    rv->addLayout(zr); rv->addLayout(er);
    auto *verifyGrid = new QGridLayout;
    verifyGrid->setContentsMargins(0, 0, 0, 0);
    verifyGrid->setHorizontalSpacing(2);
    verifyGrid->setVerticalSpacing(2);
    const QStringList verifyNames = {QStringLiteral("0°"), QStringLiteral("60°"),
                                     QStringLiteral("120°"), QStringLiteral("180°"),
                                     QStringLiteral("240°"), QStringLiteral("300°")};
    QLabel *verifyLabels[6];
    for (int i = 0; i < 6; ++i) {
        const int row = i / 2;
        const int col = (i % 2) * 2;
        auto *lbl = new QLabel(QStringLiteral("--"));
        lbl->setStyleSheet(QString("color:%1;font-size:11px;").arg(th.subText));
        lbl->setFixedWidth(44);
        verifyGrid->addWidget(new QLabel(verifyNames[i]), row, col);
        verifyGrid->addWidget(lbl, row, col + 1);
        verifyLabels[i] = lbl;
    }
    rv->addLayout(verifyGrid);
    rv->addStretch();

    // ---- 底部调试台 ----
    // 从左到右：零位校准 → 控制模式+波形按钮 → 目标值设置 → PID 参数设置
    // 波形按钮采用单选互斥逻辑，按下时发送 OPEN 命令，关闭时发送 CLOSE 命令

    auto *control = new QGroupBox(QStringLiteral("控制模式选择"));
    control->setFixedWidth(290);
    control->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto *cv = new QVBoxLayout(control);
    cv->setContentsMargins(8, 8, 8, 8);
    cv->setSpacing(6);
    auto *cmode = makeCombo(); cmode->addItems({QStringLiteral("开环模式"), QStringLiteral("电流环"), QStringLiteral("速度环"), QStringLiteral("位置环")});
    cmode->setFixedWidth(216);
    cv->addWidget(cmode, 0, Qt::AlignLeft);
    auto *wg = new QGridLayout;
    wg->setContentsMargins(0, 0, 0, 0);
    wg->setHorizontalSpacing(6);
    wg->setVerticalSpacing(6);
    wg->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    const QList<QPair<int, int>> waveCmds = {
        {static_cast<int>(SerialCommand::CMD_MECHANICALANGLE), static_cast<int>(SerialCommand::CMD_MECHANICALANGLE_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_ELECTRICALANGLE), static_cast<int>(SerialCommand::CMD_ELECTRICALANGLE_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_ADC), static_cast<int>(SerialCommand::CMD_ADC_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_IALPHA_BETA), static_cast<int>(SerialCommand::CMD_IALPHA_BETA_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_UABC), static_cast<int>(SerialCommand::CMD_UABC_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_IABC), static_cast<int>(SerialCommand::CMD_IABC_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_IQ_ID), static_cast<int>(SerialCommand::CMD_IQ_ID_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_TABC), static_cast<int>(SerialCommand::CMD_TABC_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_UALPHA_BETA), static_cast<int>(SerialCommand::CMD_UALPHA_BETA_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_SPEED), static_cast<int>(SerialCommand::CMD_SPEED_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_LOCAL), static_cast<int>(SerialCommand::CMD_LOCAL_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_IQ_ID), static_cast<int>(SerialCommand::CMD_IQ_ID_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_SPEEDOUT), static_cast<int>(SerialCommand::CMD_SPEEDOUT_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_LOCALOUT), static_cast<int>(SerialCommand::CMD_LOCALOUT_CLOSE)},
        {static_cast<int>(SerialCommand::CMD_ADCVBUS), static_cast<int>(SerialCommand::CMD_ADCVBUS_CLOSE)}
    };
    QList<QPushButton *> trendButtons;
    QStringList waves = {QStringLiteral("机械角度"), QStringLiteral("电角度"), QStringLiteral("三相ADC"), QStringLiteral("IAlpha_B..."), QStringLiteral("三相电压..."), QStringLiteral("三相电流"), QStringLiteral("IQ_ID"), QStringLiteral("三相SVP..."), QStringLiteral("UAlpha_B..."), QStringLiteral("速度"), QStringLiteral("位置"), QStringLiteral("电流环输出"), QStringLiteral("速度环输出"), QStringLiteral("位置环输出"), QStringLiteral("母线ADC")};
    const QColor trendAccent(accentBrush);
    const QString trendNormalBg = "#1f2b41";
    const QString trendNormalBorder = "#5b76a3";
    const QString trendHoverBg = "#24344f";
    const QString trendPressBg = "#19263a";
    const QString trendCheckedBg = trendAccent.isValid() ? trendAccent.name(QColor::HexRgb) : QStringLiteral("#4ba7ff");
    const QString trendCheckedHover = trendAccent.isValid() ? trendAccent.lighter(108).name(QColor::HexRgb) : QStringLiteral("#62b4ff");
    const QString trendCheckedPress = trendAccent.isValid() ? trendAccent.darker(118).name(QColor::HexRgb) : QStringLiteral("#3a8fdf");
    const QString trendStyle = QString(
        "QPushButton{background:%1;color:#eaf2ff;border:1px solid %2;border-radius:7px;padding:2px 6px;}"
        "QPushButton:hover{background:%3;border-color:%2;}"
        "QPushButton:pressed{background:%4;border-color:%2;padding-top:4px;padding-left:12px;padding-right:8px;padding-bottom:0px;}"
        "QPushButton:checked{background:%5;color:#ffffff;border:1px solid %5;}"
        "QPushButton:checked:hover{background:%6;border-color:%6;}"
        "QPushButton:checked:pressed{background:%7;border-color:%7;padding-top:4px;padding-left:12px;padding-right:8px;padding-bottom:0px;}")
        .arg(trendNormalBg, trendNormalBorder, trendHoverBg, trendPressBg, trendCheckedBg, trendCheckedHover, trendCheckedPress);
    for (int i = 0; i < waves.size(); ++i) {
        auto *b = makeBtn(waves[i], "#3f4f6d", 30);
        b->setCheckable(true);
        b->setStyleSheet(trendStyle);
        b->setFixedWidth(88);
        m_trendOpenCmd.insert(b, waveCmds[i].first);
        m_trendCloseCmd.insert(b, waveCmds[i].second);
        trendButtons.push_back(b);
        wg->addWidget(b, i / 3, i % 3);
    }
    cv->addLayout(wg);

    auto *target = new QGroupBox(QStringLiteral("目标值设置"));
    target->setFixedWidth(180);
    target->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto *tv = new QVBoxLayout(target);
    tv->setContentsMargins(6, 6, 6, 6);
    tv->setSpacing(6);
    const int kTargetInputW = 77;
    const int kTargetBtnW = 68;
    const int kTargetGapW = 6;
    QStringList tnames = {QStringLiteral("设置Uq"), QStringLiteral("设置Ud"), QStringLiteral("设置Iq"), QStringLiteral("设置Id"), QStringLiteral("设置速度"), QStringLiteral("设置位置")};
    const QList<int> tcmds = {
        static_cast<int>(SerialCommand::CMD_SETUQ),
        static_cast<int>(SerialCommand::CMD_SETUD),
        static_cast<int>(SerialCommand::CMD_SETIQ),
        static_cast<int>(SerialCommand::CMD_SETID),
        static_cast<int>(SerialCommand::CMD_SETSPEEDTAR),
        static_cast<int>(SerialCommand::CMD_SETLOCALTAR)
    };
    for (int i = 0; i < tnames.size(); ++i) {
        auto *r = new QHBoxLayout;
        r->setSpacing(0);
        auto *edit = makeInput("0.0");
        auto *btn = makeBtn(tnames[i], accentBrush, 28);
        auto *gap = new QWidget;
        gap->setFixedWidth(kTargetGapW);
        gap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        edit->setFixedWidth(kTargetInputW);
        btn->setFixedWidth(kTargetBtnW);
        r->addWidget(edit);
        r->addWidget(gap);
        r->addWidget(btn);
        tv->addLayout(r);
        m_commandEdits.insert(tcmds[i], edit);
        connect(btn, &QPushButton::clicked, this, [this, edit, i, tcmds]() {
            if (!m_serial || !m_serial->isConnected()) {
                if (m_serial) {
                    m_serial->playSystemAlert();
                }
                showSerialNotOpenTipDialog(this);
                return;
            }
            m_serial->sendFloatCommand(tcmds[i], QLocale().toDouble(edit->text()));
        });
    }
    tv->addStretch();

    auto *pid = new QGroupBox(QStringLiteral("PID 参数设置"));
    pid->setMinimumWidth(418);
    pid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *pg = new QHBoxLayout(pid);
    pg->setContentsMargins(2, 2, 2, 4);
    pg->setSpacing(3);
    const int kPidInputW = 80;
    const int kPidBtnW = 68;
    const int kPidGapW = 6;
    auto addPid = [&](int col, const QString &titleText, const QStringList &rows, const QList<int> &cmds) {
        Q_UNUSED(col);
        auto *box = new QGroupBox(titleText);
        box->setFixedWidth(176);
        box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto *v = new QVBoxLayout(box);
        v->setContentsMargins(4, 6, 4, 6);
        v->setSpacing(6);
        for (int i = 0; i < rows.size(); ++i) {
            auto *h = new QHBoxLayout;
            h->setSpacing(0);
            auto *edit = makeInput("0.0");
            auto *btn = makeBtn(rows[i], accentBrush, 28);
            auto *gap = new QWidget;
            gap->setFixedWidth(kPidGapW);
            gap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            edit->setFixedWidth(kPidInputW);
            btn->setFixedWidth(kPidBtnW);
            h->addWidget(edit);
            h->addWidget(gap);
            h->addWidget(btn);
            v->addLayout(h);
            m_commandEdits.insert(cmds[i], edit);
            connect(btn, &QPushButton::clicked, this, [this, edit, i, cmds]() {
                if (!m_serial || !m_serial->isConnected()) {
                    if (m_serial) {
                        m_serial->playSystemAlert();
                    }
                    showSerialNotOpenTipDialog(this);
                    return;
                }
                m_serial->sendFloatCommand(cmds[i], QLocale().toDouble(edit->text()));
            });
        }
        pg->addWidget(box, 0, Qt::AlignTop);
    };
    addPid(0, QStringLiteral("电流环PID参数整定"), {QStringLiteral("设置KP"), QStringLiteral("设置KI"), QStringLiteral("输出限制")},
           {static_cast<int>(SerialCommand::CMD_SETIQPIDKP), static_cast<int>(SerialCommand::CMD_SETIQPIDKI), static_cast<int>(SerialCommand::CMD_SETIQPIDOUT)});
    addPid(1, QStringLiteral("速度环PID参数整定"), {QStringLiteral("设置KP"), QStringLiteral("设置KI"), QStringLiteral("输出限制")},
           {static_cast<int>(SerialCommand::CMD_SETSPEEDPIDKP), static_cast<int>(SerialCommand::CMD_SETSPEEDPIDKI), static_cast<int>(SerialCommand::CMD_SETSPEEDPIDOUT)});
    addPid(2, QStringLiteral("位置环PID参数整定"), {QStringLiteral("设置KP"), QStringLiteral("设置KD"), QStringLiteral("输出限制")},
           {static_cast<int>(SerialCommand::CMD_SETLOCALPIDKP), static_cast<int>(SerialCommand::CMD_SETLOCALPIDKD), static_cast<int>(SerialCommand::CMD_SETLOCALPIDOUT)});

    bottomL->addWidget(reserve);
    bottomL->addWidget(control);
    bottomL->addSpacing(2);
    bottomL->addWidget(target);
    bottomL->addWidget(pid, 1);

    rightV->addWidget(chartCard, 2);
    rightV->addWidget(bottomCard, 1);

    body->addWidget(left);
    body->addWidget(right, 1);
    mainV->addLayout(body, 1);

    m_portCombo = com;
    m_baudCombo = baud;
    m_openCloseBtn = openSerialBtn;
    m_statusLabel = status;
    m_logBtn = logBtn;
    m_serialLamp = lamp;
    m_motorLamp = mlamp;
    m_motorConnectBtn = motorConnectBtn;
    m_zeroCalibBtn = zeroCalibBtn;
    m_zeroOffsetEdit = zeroOffsetEdit;
    m_elecAngleEdit = elecAngleEdit;
    for (int i = 0; i < 6; ++i) m_verifyLabels[i] = verifyLabels[i];
    m_ctrlModeCombo = cmode;
    m_mosTempText = mosVal;
    m_mosTempSlider = mos;
    m_plotRangeSlider = plotRangeSlider;
    m_polePairsEdit = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETPAIRS));
    m_angleDirEdit = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETDIR));
    m_speedDirEdit = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETSPEEDDIR));

    connect(plotRangeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (!m_plotManager) {
            return;
        }
        const double minRange = 0.1;
        const double maxRange = 10.0;
        const double rangeSec = minRange + (maxRange - minRange) * value / 100.0;
        m_plotManager->setXAxisRange(rangeSec);
    });
    plotRangeSlider->valueChanged(plotRangeSlider->value());

    connect(openSerialBtn, &QPushButton::clicked, this, [this, openSerialBtn]() {
        if (!m_serial) {
            return;
        }
        if (m_serial->isConnected()) {
            m_serial->disconnectPort();
            return;
        }
        m_serial->refreshPorts();
        const QString port = m_portCombo ? m_portCombo->currentText() : QString();
        const int baud = m_baudCombo ? m_baudCombo->currentText().toInt() : 115200;
        m_serial->connectPort(port, baud);
        openSerialBtn->setText(m_serial->isConnected() ? QStringLiteral("关闭串口") : QStringLiteral("打开串口"));
    });
    connect(minimizeBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(logBtn, &QPushButton::clicked, this, [this]() {
        if (!m_logDialog) {
            m_logDialog = new QDialog(this);
            m_logDialog->setWindowTitle(QStringLiteral("串口日志"));
            m_logDialog->resize(520, 220);
            auto *v = new QVBoxLayout(m_logDialog);
            m_logText = new QTextEdit(m_logDialog);
            m_logText->setReadOnly(true);
            m_logText->setPlainText(QStringLiteral("为避免高频日志导致界面卡顿，串口日志输出已关闭。"));
            v->addWidget(m_logText);
            connect(m_logDialog, &QDialog::finished, this, [this]() {
                if (m_serial) {
                    m_serial->setRxLogEnabled(false);
                }
            });
        }
        if (m_logText) {
            m_logText->setPlainText(QStringLiteral("为避免高频日志导致界面卡顿，串口日志输出已关闭。"));
        }
        if (m_serial) {
            m_serial->setRxLogEnabled(false);
        }
        m_logDialog->show();
        m_logDialog->raise();
        m_logDialog->activateWindow();
    });
    connect(motorConnectBtn, &QPushButton::clicked, this, [this]() {
        if (!m_serial || !m_serial->isConnected()) {
            if (m_serial) {
                m_serial->playSystemAlert();
            }
            showSerialNotOpenTipDialog(this);
            return;
        }
        m_serial->connectMotor();
    });
    connect(zeroCalibBtn, &QPushButton::clicked, this, [this, zeroCalibBtn](bool checked) {
        if (!m_serial || !m_serial->isConnected()) {
            zeroCalibBtn->blockSignals(true);
            zeroCalibBtn->setChecked(!checked);
            zeroCalibBtn->blockSignals(false);
            if (m_serial) {
                m_serial->playSystemAlert();
            }
            showSerialNotOpenTipDialog(this);
            return;
        }
        const int cmd = checked
            ? static_cast<int>(SerialCommand::CMD_ZEROCALIBRATIO)
            : static_cast<int>(SerialCommand::CMD_ZEROCALIBRATIO_OVER);
        m_serial->sendFloatCommand(cmd, 0.0);
    });
    connect(verifyBtn, &QPushButton::clicked, this, [this]() {
        if (!m_serial || !m_serial->isConnected()) {
            if (m_serial) m_serial->playSystemAlert();
            showSerialNotOpenTipDialog(this);
            return;
        }
        for (int i = 0; i < 6; ++i) {
            if (m_verifyLabels[i]) m_verifyLabels[i]->setText(QStringLiteral("..."));
        }
        m_serial->sendFloatCommand(static_cast<int>(SerialCommand::CMD_VERIFY_OFFSET), 0.0);
    });
    connect(cmode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (!m_serial || !m_serial->isConnected()) {
            return;
        }
        static const QList<int> modeCmds = {
            static_cast<int>(SerialCommand::CMD_OPEN_LOOP),
            static_cast<int>(SerialCommand::CMD_CURRENT_LOOP),
            static_cast<int>(SerialCommand::CMD_SPEED_LOOP),
            static_cast<int>(SerialCommand::CMD_POSITION_LOOP)
        };
        if (idx >= 0 && idx < modeCmds.size()) {
            m_serial->sendFloatCommand(modeCmds[idx], static_cast<double>(idx));
        }
    });
    // ---- 波形订阅按钮通用信号绑定 ----
    // 规则：单选互斥（打开新波形自动关闭旧波形）
    for (auto it = m_trendOpenCmd.constBegin(); it != m_trendOpenCmd.constEnd(); ++it) {
        QPushButton *btn = it.key();
        connect(btn, &QPushButton::clicked, this, [this, btn](bool checked) {
            if (!m_serial || !m_serial->isConnected()) {
                btn->blockSignals(true);
                btn->setChecked(!checked);
                btn->blockSignals(false);
                if (m_serial) {
                    m_serial->playSystemAlert();
                }
                showSerialNotOpenTipDialog(this);
                return;
            }
            const int openCmd = m_trendOpenCmd.value(btn, -1);
            const int closeCmd = m_trendCloseCmd.value(btn, -1);
            if (checked) {
                m_serial->sendFloatCommand(openCmd, 0.0);
            } else {
                if (closeCmd > 0) {
                    m_serial->sendFloatCommand(closeCmd, 0.0);
                }
            }
        });
    }
    connect(m_serial, &SerialManager::availablePortsChanged, this, [this]() {
        if (!m_portCombo || !m_serial) {
            return;
        }
        const QString keep = m_portCombo->currentText();
        m_portCombo->clear();
        m_portCombo->addItems(m_serial->availablePorts());
        const int idx = m_portCombo->findText(keep);
        if (idx >= 0) {
            m_portCombo->setCurrentIndex(idx);
        }
    });
    connect(m_serial, &SerialManager::connectedChanged, this, [this]() {
        if (!m_serial || !m_openCloseBtn || !m_serialLamp) {
            return;
        }
        m_openCloseBtn->setText(m_serial->isConnected() ? QStringLiteral("关闭串口") : QStringLiteral("打开串口"));
        m_serialLamp->setStyleSheet(m_serial->isConnected()
            ? "border-radius:9px;background:#4bd488;border:1px solid #d7deea;"
            : "border-radius:9px;background:#9da7b6;border:1px solid #d7deea;");
    });
    connect(m_serial, &SerialManager::motorConnectedChanged, this, [this]() {
        if (!m_serial || !m_motorLamp) {
            return;
        }
        m_motorLamp->setStyleSheet(m_serial->isMotorConnected()
            ? "border-radius:9px;background:#4bd488;border:1px solid #d7deea;"
            : "border-radius:9px;background:#9da7b6;border:1px solid #d7deea;");
    });
    connect(m_serial, &SerialManager::statusMessageChanged, this, [this, th]() {
        if (!m_statusLabel || !m_serial) {
            return;
        }
        m_statusLabel->setText(QStringLiteral("状态：") + m_serial->statusMessage());
        const QString statusTop = QColor(th.cardBg).lighter(118).name(QColor::HexRgb);
        const QString statusBorder = QColor(th.border).lighter(118).name(QColor::HexRgb);
        m_statusLabel->setStyleSheet(QString(
            "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %1, stop:1 %2);"
            "border:2px solid %3;border-radius:12px;padding:6px 10px;color:%4;font-weight:700;")
            .arg(statusTop, th.cardBg, statusBorder, th.text));
    });
    connect(m_serial, &SerialManager::mosTemperatureChanged, this, [this]() {
        if (!m_serial || !m_mosTempText || !m_mosTempSlider) {
            return;
        }
        const double t = m_serial->mosTemperature();
        m_mosTempText->setText(formatCompactNumber(t, 1) + QStringLiteral(" ℃"));
        m_mosTempSlider->setValue(qBound(0, static_cast<int>(qRound(t)), 120));
    });
    connect(m_serial, &SerialManager::dataReceived, this, [this](const QString &) {
        // 串口日志输出禁用，避免高频 append 造成 UI 卡顿。
    });
    // ---- 串口帧解析信号处理（核心数据路由） ----
    // CMD_CONNECT_MOTOR 回包：将下位机当前参数回填到各输入框
    //   values[0-13]：极对数、方向、零偏值、PID 系数等
    //   values[14-16]（扩展）：电机 Rs/Lq/Ld，仅回包长度≥17 时回填
    // CMD_ZEROCALIBRATIO_OVER 回包：回填零偏值和电角度
    // 其他命令：通过 appendTrendValues 分发到图表曲线
    connect(m_serial, &SerialManager::frameParsed, this, [this, zeroCalibBtn](int command, const QVariantList &values) {
        if (command == static_cast<int>(SerialCommand::CMD_CONNECT_MOTOR) && values.size() >= 14) {
            if (m_polePairsEdit) m_polePairsEdit->setText(formatCompactNumber(values[0].toDouble()));
            if (m_angleDirEdit) m_angleDirEdit->setText(formatCompactNumber(values[1].toDouble()));
            if (m_zeroOffsetEdit) m_zeroOffsetEdit->setText(formatCompactNumber(values[2].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETIQPIDKP))) e->setText(formatCompactNumber(values[3].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETIQPIDKI))) e->setText(formatCompactNumber(values[4].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETSPEEDDIR))) e->setText(formatCompactNumber(values[6].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETSPEEDPIDKP))) e->setText(formatCompactNumber(values[7].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETSPEEDPIDKI))) e->setText(formatCompactNumber(values[8].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETLOCALPIDKP))) e->setText(formatCompactNumber(values[9].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETLOCALPIDKD))) e->setText(formatCompactNumber(values[10].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETIQPIDOUT))) e->setText(formatCompactNumber(values[11].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETSPEEDPIDOUT))) e->setText(formatCompactNumber(values[12].toDouble()));
            if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETLOCALPIDOUT))) e->setText(formatCompactNumber(values[13].toDouble()));
            if (values.size() >= 17) {
                if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETMOTORRS))) e->setText(formatCompactNumber(values[14].toDouble()));
                if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETMOTORLQ))) e->setText(formatCompactNumber(values[15].toDouble()));
                if (auto *e = m_commandEdits.value(static_cast<int>(SerialCommand::CMD_SETMOTORLD))) e->setText(formatCompactNumber(values[16].toDouble()));
            }
        } else if (command == static_cast<int>(SerialCommand::CMD_ZEROCALIBRATIO_OVER) && values.size() >= 2) {
            if (m_zeroOffsetEdit) m_zeroOffsetEdit->setText(formatCompactNumber(values[0].toDouble()));
            if (m_elecAngleEdit) m_elecAngleEdit->setText(formatCompactNumber(values[1].toDouble()));
            if (zeroCalibBtn) {
                zeroCalibBtn->blockSignals(true);
                zeroCalibBtn->setChecked(false);
                zeroCalibBtn->blockSignals(false);
            }
        } else if (command == static_cast<int>(SerialCommand::CMD_VERIFY_OFFSET) && values.size() >= 6) {
            for (int i = 0; i < 6; ++i) {
                if (m_verifyLabels[i]) {
                    double deg = values[i].toDouble() * 57.29578;
                    m_verifyLabels[i]->setText(QString::number(deg, 'f', 1) + QStringLiteral("°"));
                }
            }
        }
        appendTrendValues(command, values);
    });
    if (m_serial) {
        m_serial->setRxLogEnabled(false);
        m_serial->refreshPorts();
        if (m_portCombo) {
            m_portCombo->clear();
            m_portCombo->addItems(m_serial->availablePorts());
        }
    }

    // 设置对话框：按参考工程改为同页“设置 + 主题与颜色 + 开发者留言”布局
    connect(settingsBtn, &QPushButton::clicked, this, [this, themes, accents]() {
        auto themeAt = [&themes](int idx) {
            return themes[qBound(0, idx, themes.size() - 1)];
        };
        auto accentAt = [&accents](int idx) {
            return accents[qBound(0, idx, accents.size() - 1)];
        };
        auto th = themeAt(m_themeIndex);
        auto accent = accentAt(m_accentIndex);

        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("系统设置"));
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg.setModal(true);
        dlg.resize(460, 390);

        auto *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(8);

        auto *header = new QFrame;
        header->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}").arg(th.panelBg, th.border));
        auto *headerL = new QHBoxLayout(header);
        headerL->setContentsMargins(10, 6, 6, 6);
        headerL->setSpacing(8);
        auto *dot = new QLabel(QStringLiteral("◉"));
        dot->setStyleSheet(QString("QLabel{color:%1;font-size:13px;font-weight:700;}").arg(accent));
        auto *headerTitle = new QLabel(QStringLiteral("系统设置"));
        headerTitle->setStyleSheet(QString("QLabel{color:%1;font-size:14px;font-weight:700;}").arg(th.text));
        auto *headerClose = makeBtn(QStringLiteral("✕"), accent, 26);
        headerClose->setFixedSize(28, 26);
        headerL->addWidget(dot);
        headerL->addWidget(headerTitle);
        headerL->addStretch();
        headerL->addWidget(headerClose);
        root->addWidget(header);
        connect(headerClose, &QPushButton::clicked, &dlg, &QDialog::reject);

        auto *stack = new QStackedWidget;
        root->addWidget(stack, 1);

        auto *mainPage = new QWidget;
        auto *mainL = new QVBoxLayout(mainPage);
        mainL->setContentsMargins(0, 0, 0, 0);
        mainL->setSpacing(8);

        auto *mainTitle = new QLabel(QStringLiteral("设置"));
        mainTitle->setStyleSheet(QString("font-size:30px;font-weight:700;color:%1;").arg(th.text));
        mainL->addWidget(mainTitle);

        auto *hint = new QLabel(QStringLiteral("选择你要修改的设置"));
        hint->setStyleSheet(QString("color:%1;font-size:13px;").arg(th.subText));
        mainL->addWidget(hint);

        auto *themeEntryBtn = makeBtn(QStringLiteral("主题与颜色"), accent, 34);
        themeEntryBtn->setStyleSheet(makeButtonStyle(accent, th.text, th.border, 6));
        mainL->addWidget(themeEntryBtn);

        auto *devBtn = makeBtn(QStringLiteral("开发者留言"), accent, 30);
        devBtn->setStyleSheet(makeButtonStyle(accent, th.text, th.border, 6));
        mainL->addWidget(devBtn);
        mainL->addStretch();

        auto *themePage = new QWidget;
        auto *themePageL = new QVBoxLayout(themePage);
        themePageL->setContentsMargins(0, 0, 0, 0);
        themePageL->setSpacing(8);

        auto *headRow = new QHBoxLayout;
        auto *backBtn = makeBtn(QStringLiteral("返回"), accent, 28);
        backBtn->setFixedWidth(68);
        backBtn->setStyleSheet(makeButtonStyle(accent, th.text, th.border, 6));
        auto *themePageTitle = new QLabel(QStringLiteral("主题与颜色"));
        themePageTitle->setStyleSheet(QString("font-size:18px;font-weight:700;color:%1;").arg(th.text));
        headRow->addWidget(backBtn);
        headRow->addWidget(themePageTitle);
        headRow->addStretch();
        themePageL->addLayout(headRow);

        auto *themeBox = new QGroupBox(QStringLiteral("主题设置"));
        auto *themeL = new QGridLayout(themeBox);
        themeL->setContentsMargins(10, 12, 10, 10);
        themeL->setHorizontalSpacing(8);
        themeL->setVerticalSpacing(6);
        auto *themeCombo = makeCombo();
        themeCombo->addItems({QStringLiteral("蓝色"), QStringLiteral("浅色"), QStringLiteral("纯黑"), QStringLiteral("绿色"), QStringLiteral("暖色"), QStringLiteral("紫色")});
        themeCombo->setCurrentIndex(m_themeIndex);
        auto *accentCombo = makeCombo();
        accentCombo->addItems({QStringLiteral("蓝色"), QStringLiteral("绿色"), QStringLiteral("橙色"), QStringLiteral("红色")});
        accentCombo->setCurrentIndex(m_accentIndex);

        auto makeDarkLabel = [](const QString &text) {
            auto *l = new QLabel(text);
            l->setMinimumHeight(28);
            return l;
        };
        const QString settingLabelStyle = QString("QLabel{background:%1;color:%2;border:1px solid %3;border-radius:2px;padding-left:8px;}")
                                              .arg(th.panelBg, th.text, th.border);

        themeCombo->setStyleSheet(QString(
            "QComboBox{background:%1;color:%2;border:1px solid %3;border-radius:6px;padding:0 28px 0 8px;min-height:28px;}"
            "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:24px;border:none;}"
            "QComboBox::down-arrow{image:none;width:0px;height:0px;}")
            .arg(th.cardBg, th.text, th.border));
        accentCombo->setStyleSheet(QString(
            "QComboBox{background:%1;color:%2;border:1px solid %3;border-radius:6px;padding:0 28px 0 8px;min-height:28px;}"
            "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:24px;border:none;}"
            "QComboBox::down-arrow{image:none;width:0px;height:0px;}")
            .arg(th.cardBg, th.text, th.border));

        auto *themeLabel = makeDarkLabel(QStringLiteral("主题风格"));
        themeLabel->setStyleSheet(settingLabelStyle);
        themeL->addWidget(themeLabel, 0, 0);
        themeL->addWidget(themeCombo, 0, 1);
        auto *accentLabel = makeDarkLabel(QStringLiteral("强调色"));
        accentLabel->setStyleSheet(settingLabelStyle);
        themeL->addWidget(accentLabel, 1, 0);
        themeL->addWidget(accentCombo, 1, 1);
        themeL->setColumnStretch(0, 1);
        themeL->setColumnStretch(1, 1);
        themePageL->addWidget(themeBox);
        themePageL->addStretch();

        stack->addWidget(mainPage);
        stack->addWidget(themePage);
        stack->setCurrentWidget(mainPage);

        auto *closeRow = new QHBoxLayout;
        closeRow->addStretch();
        auto *closeBtn = makeBtn(QStringLiteral("Close"), accent, 26);
        closeBtn->setFixedWidth(76);
        closeBtn->setStyleSheet(makeButtonStyle(accent, th.text, th.border, 6));
        closeRow->addWidget(closeBtn);
        root->addLayout(closeRow);
        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(themeEntryBtn, &QPushButton::clicked, &dlg, [stack, themePage]() {
            stack->setCurrentWidget(themePage);
        });
        connect(backBtn, &QPushButton::clicked, &dlg, [stack, mainPage]() {
            stack->setCurrentWidget(mainPage);
        });

        auto applyDialogTheme = [&]() {
            const auto t = themeAt(m_themeIndex);
            const QString a = accentAt(m_accentIndex);
            const QString aBg = a;
            dlg.setStyleSheet(QString(
                "QDialog{background:%1;border:1px solid %4;border-radius:14px;}"
                "QLabel{color:%2;}"
                "QGroupBox{background:%3;border:1px solid %4;border-radius:8px;margin-top:8px;padding-top:8px;}"
                "QGroupBox::title{subcontrol-origin:margin;left:9px;padding:0 4px;color:%2;font-weight:500;}")
                .arg(t.windowBg, t.text, t.cardBg, t.border));
            header->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:10px;}").arg(t.panelBg, t.border));
            dot->setStyleSheet(QString("QLabel{color:%1;font-size:13px;font-weight:700;}").arg(a));
            headerTitle->setStyleSheet(QString("QLabel{color:%1;font-size:14px;font-weight:700;}").arg(t.text));
            headerClose->setStyleSheet(makeButtonStyle(aBg, t.text, t.border, 8));
            mainTitle->setStyleSheet(QString("font-size:30px;font-weight:700;color:%1;").arg(t.text));
            hint->setStyleSheet(QString("color:%1;font-size:13px;").arg(t.subText));
            themeEntryBtn->setStyleSheet(makeButtonStyle(aBg, t.text, t.border, 6));
            devBtn->setStyleSheet(makeButtonStyle(aBg, t.text, t.border, 6));
            backBtn->setStyleSheet(makeButtonStyle(aBg, t.text, t.border, 6));
            themePageTitle->setStyleSheet(QString("font-size:18px;font-weight:700;color:%1;").arg(t.text));
            const QString settingLabelStyleNow = QString("QLabel{background:%1;color:%2;border:1px solid %3;border-radius:2px;padding-left:8px;}")
                                                     .arg(t.panelBg, t.text, t.border);
            themeLabel->setStyleSheet(settingLabelStyleNow);
            accentLabel->setStyleSheet(settingLabelStyleNow);
            themeCombo->setStyleSheet(QString(
                "QComboBox{background:%1;color:%2;border:1px solid %3;border-radius:6px;padding:0 28px 0 8px;min-height:28px;}"
                "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:24px;border:none;}"
                "QComboBox::down-arrow{image:none;width:0px;height:0px;}")
                .arg(t.cardBg, t.text, t.border));
            accentCombo->setStyleSheet(QString(
                "QComboBox{background:%1;color:%2;border:1px solid %3;border-radius:6px;padding:0 28px 0 8px;min-height:28px;}"
                "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:24px;border:none;}"
                "QComboBox::down-arrow{image:none;width:0px;height:0px;}")
                .arg(t.cardBg, t.text, t.border));
            closeBtn->setStyleSheet(makeButtonStyle(aBg, t.text, t.border, 6));
        };
        applyDialogTheme();

        auto scheduleRebuild = [this](bool reopenSettingsDialog) {
            if (m_themeRebuildScheduled) {
                return;
            }
            m_themeRebuildScheduled = true;
            QTimer::singleShot(0, this, [this, reopenSettingsDialog]() {
                m_themeRebuildScheduled = false;
                buildUi();
                if (reopenSettingsDialog && m_settingsBtn) {
                    QTimer::singleShot(0, this, [this]() {
                        if (m_settingsBtn) {
                            m_settingsBtn->click();
                        }
                    });
                }
            });
        };
        connect(themeCombo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, [this, applyDialogTheme, scheduleRebuild](int idx) {
            if (idx == m_themeIndex) {
                return;
            }
            m_themeIndex = idx;
            applyDialogTheme();
            scheduleRebuild(false);
        });
        connect(accentCombo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, [this, applyDialogTheme, scheduleRebuild](int idx) {
            if (idx == m_accentIndex) {
                return;
            }
            m_accentIndex = idx;
            applyDialogTheme();
            scheduleRebuild(false);
        });

        connect(devBtn, &QPushButton::clicked, &dlg, [this, accent]() {
            QDialog note(this);
            note.setWindowTitle(QStringLiteral("开发者留言"));
            note.setModal(true);
            note.resize(620, 520);
            auto *v = new QVBoxLayout(&note);
            auto *browser = new QTextBrowser;
            browser->setOpenExternalLinks(true);
            browser->setStyleSheet("QTextBrowser{border:1px solid #4f5f7d;border-radius:8px;padding:8px;}");
            browser->setHtml(
                QString("<h3>开发者留言</h3>"
                        "<p>欢迎使用 LiJointMaster2，你的贴心 FOC 电机控制小帮手！</p>"
                        "<p><b>作者：</b>李环文<br><b>QQ：</b>2937739768<br><b>版本：</b>1.0<br><b>日期：</b>2025-12-02</p>"
                        "<p><b>主要功能：</b><br>"
                        "• 串口热插拔与自动断连<br>"
                        "• 实时趋势图<br>"
                        "• 参数操作面板<br>"
                        "• 主题与强调色设置</p>"
                        "<p><b>Git：</b><br>"
                        "<a href='https://github.com/lhzloveyyj/LiJointMaster2.git'>https://github.com/lhzloveyyj/LiJointMaster2.git</a></p>"
                        "<p>祝你 FOC 开心！</p>")
                    );
            v->addWidget(browser);
            auto *close = makeBtn(QStringLiteral("关闭"), accent, 30);
            v->addWidget(close, 0, Qt::AlignRight);
            connect(close, &QPushButton::clicked, &note, &QDialog::accept);
            note.exec();
        });

        dlg.exec();
    });
}
