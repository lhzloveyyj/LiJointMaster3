#include "serialmanager.h"

/**
 * @file serialmanager.cpp
 * @brief 串口管理器实现
 *
 * 实现串口枚举、连接管理、协议编解码的核心逻辑。
 * 通信协议：
 *   - 发送：固定 8 字节帧 [0xA5][CMD][4B float][SUM][0x49]
 *   - 接收：可变帧 [0xA5][CMD][LEN][载荷(4的倍数)][SUM][0x49]
 *
 * 本文件包含粘包处理、帧同步、校验和验证等协议层逻辑。
 */

#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QtGlobal>

#if LIJOINT_HAS_SERIALPORT
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <cstring>
#include <cstdio>

#include "serialcommand.h"

namespace {
/**
 * @brief 接收帧载荷最大长度（单位：字节）
 *
 * 防止异常数据帧撑爆解析缓冲区。
 * 128 字节可容纳 32 个 float，满足所有已知回包类型。
 */
constexpr int kMaxPayloadLen = 128;

/**
 * @brief 标准化串口名
 *
 * Linux 下 "/dev/ttyUSB0" → "ttyUSB0"，
 * Windows/Mac 下保持原名不变。
 */
QString normalizedPortName(const QString &portName)
{
#ifdef Q_OS_WIN
    if (portName.startsWith(QStringLiteral("\\\\.\\"))) {
        return portName.mid(4);
    }
#endif
    if (portName.startsWith(QStringLiteral("/dev/"))) {
        return QFileInfo(portName).fileName();
    }
    return portName;
}

/**
 * @brief 检查串口列表是否包含指定串口（支持多种命名格式）
 */
bool containsPort(const QStringList &ports, const QString &portName)
{
    const QString normalized = normalizedPortName(portName);
    return ports.contains(portName)
        || ports.contains(normalized)
        || ports.contains(QStringLiteral("/dev/") + normalized);
}
}

/**
 * @brief 构造函数
 *
 * 初始化串口对象、信号绑定、热插拔轮询定时器（300ms）。
 */
SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
#if LIJOINT_HAS_SERIALPORT
    , m_serialPort(new QSerialPort(this))
#endif
    , m_motorConnected(false)
    , m_mosTemperature(0.0)
    , m_activeTrendCommand(0)
    , m_portWatchTimer(new QTimer(this))
    , m_rxLogEnabled(false)
{
#if LIJOINT_HAS_SERIALPORT
    // 串口数据到达信号 → 读取并解析
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialManager::readSerialData);

    // 串口错误信号 → 更新状态文本
    connect(m_serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) {
            return;
        }
        setStatusMessage(QStringLiteral("串口错误: %1").arg(m_serialPort->errorString()));
    });
#endif

    // 定时轮询串口列表 → 热插拔检测
    connect(m_portWatchTimer, &QTimer::timeout, this, [this]() {
        updateAvailablePorts(false);
    });
    m_portWatchTimer->setInterval(300);
    m_portWatchTimer->start();

    // 首次刷新串口列表
    updateAvailablePorts(true);
}

/**
 * @brief 析构函数，确保关闭串口
 */
SerialManager::~SerialManager()
{
    disconnectPort();
}

/* ---------- 属性读取 ---------- */
QStringList SerialManager::availablePorts() const { return m_availablePorts; }
bool SerialManager::isConnected() const
{
#if LIJOINT_HAS_SERIALPORT
    return m_serialPort->isOpen();
#else
    return false;
#endif
}
bool SerialManager::isMotorConnected() const { return m_motorConnected; }
QString SerialManager::statusMessage() const { return m_statusMessage; }
double SerialManager::mosTemperature() const { return m_mosTemperature; }
int SerialManager::activeTrendCommand() const { return m_activeTrendCommand; }

void SerialManager::refreshPorts()
{
    updateAvailablePorts(true);
}

/**
 * @brief 更新可用串口列表（热插拔核心逻辑）
 *
 * - 通过 QSerialPortInfo 枚举系统串口
 * - 在 Linux 下额外扫描 /dev/ttyUSB* 和 /dev/ttyACM*
 * - USB/ACM 设备优先排序
 * - 若已连接串口被移除，自动执行状态复位
 * - 检测到列表变化时触发 availablePortsChanged 信号
 *
 * @param manualRefresh 是否由用户手动触发（true 时显示提示信息）
 */
void SerialManager::updateAvailablePorts(bool manualRefresh)
{
#if !LIJOINT_HAS_SERIALPORT
    if (!m_availablePorts.isEmpty()) {
        m_availablePorts.clear();
        emit availablePortsChanged();
    }

    if (manualRefresh) {
        setStatusMessage(QStringLiteral("当前构建未启用 Qt SerialPort，无法枚举串口"));
    }
    return;
#else
    QStringList preferredPorts;  // USB/ACM 设备，优先显示
    QStringList otherPorts;      // 其他串口设备
    const auto portInfos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : portInfos) {
        QString location;
#ifdef Q_OS_WIN
        location = info.portName();
#else
        location = info.systemLocation().isEmpty() ? info.portName() : info.systemLocation();
#endif
        if (location.contains(QStringLiteral("ttyUSB")) || location.contains(QStringLiteral("ttyACM"))) {
            preferredPorts << location;
        } else {
            otherPorts << location;
        }
    }

#ifdef Q_OS_LINUX
    // Linux 下补充扫描 /dev 目录，解决部分系统 QSerialPortInfo 枚举不全的问题
    const QStringList fallbackFilters{
        QStringLiteral("ttyACM*"),
        QStringLiteral("ttyUSB*")
    };
    const auto fallbackEntries = QDir(QStringLiteral("/dev")).entryInfoList(
        fallbackFilters,
        QDir::System | QDir::Files | QDir::NoSymLinks,
        QDir::Name);
    for (const QFileInfo &entry : fallbackEntries) {
        const QString location = entry.absoluteFilePath();
        if (!preferredPorts.contains(location) && !otherPorts.contains(location)) {
            preferredPorts << location;
        }
    }
#endif

    preferredPorts.sort();
    otherPorts.sort();
    const QStringList ports = preferredPorts + otherPorts;

    // 热插拔检测：已连接串口被拔出时自动断连并复位所有状态
    if (isConnected() && !containsPort(ports, m_serialPort->portName())) {
        const QString removedPort = m_serialPort->portName();
        m_serialPort->close();
        m_rxBuffer.clear();
        setMotorConnected(false);
        setMosTemperature(0.0);
        setActiveTrendCommand(0);
        emit connectedChanged();
        setStatusMessage(QStringLiteral("检测到串口已移除: %1").arg(removedPort));
    }

    // 列表变化时通知 UI 刷新
    if (ports != m_availablePorts) {
        m_availablePorts = ports;
        emit availablePortsChanged();
        if (!manualRefresh) {
            setStatusMessage(QStringLiteral("检测到串口列表变化"));
            return;
        }
    }

    if (manualRefresh) {
        setStatusMessage(QStringLiteral("已刷新串口列表"));
    }
#endif
}

/**
 * @brief 打开串口连接
 *
 * 参数固定为 8N1（8 数据位、无校验、1 停止位）、无流控。
 * 打开成功时复位所有状态（电机连接、MOS 温度、活动波形等）。
 *
 * @param portName 串口名（如 "ttyUSB0" 或 "/dev/ttyUSB0"）
 * @param baudRate 波特率
 * @return true  连接成功
 * @return false 连接失败
 */
bool SerialManager::connectPort(const QString &portName, int baudRate)
{
#if !LIJOINT_HAS_SERIALPORT
    Q_UNUSED(portName);
    Q_UNUSED(baudRate);
    setStatusMessage(QStringLiteral("打开串口失败: 当前 Qt 环境缺少 SerialPort 模块"));
    return false;
#else
    // 串口参数固定为 8N1，无流控（与原项目一致）
    if (portName.isEmpty()) {
        setStatusMessage(QStringLiteral("串口名不能为空"));
        return false;
    }

    if (isConnected()) {
        disconnectPort();
    }

    m_serialPort->setPortName(normalizedPortName(portName));
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    const bool opened = m_serialPort->open(QIODevice::ReadWrite);
    emit connectedChanged();

    if (opened) {
        m_rxBuffer.clear();
        setMotorConnected(false);
        setMosTemperature(0.0);
        setActiveTrendCommand(0);
        setStatusMessage(QStringLiteral("已连接 %1 @ %2").arg(portName).arg(baudRate));
        return true;
    }

    setStatusMessage(QStringLiteral("连接失败: %1 (%2)").arg(m_serialPort->errorString(), portName));
    return false;
#endif
}

/**
 * @brief 断开串口连接
 *
 * 关闭串口、清空接收缓冲区、复位所有状态。
 */
void SerialManager::disconnectPort()
{
#if !LIJOINT_HAS_SERIALPORT
    m_rxBuffer.clear();
    setMotorConnected(false);
    setMosTemperature(0.0);
    setActiveTrendCommand(0);
    return;
#else
    if (!isConnected()) {
        return;
    }

    m_serialPort->close();
    m_rxBuffer.clear();
    setMotorConnected(false);
    setMosTemperature(0.0);
    setActiveTrendCommand(0);
    emit connectedChanged();
    setStatusMessage(QStringLiteral("串口已断开"));
#endif
}

/**
 * @brief 发送电机连接命令
 *
 * 发送 CMD_CONNECT_MOTOR（附带固定值 0.12）。
 * 下位机回包触发参数回填（见 frameParsed 信号处理）。
 *
 * @return true 命令发送成功
 */
bool SerialManager::connectMotor()
{
    if (!isConnected()) {
        setStatusMessage(QStringLiteral("连接电机失败: 串口未连接"));
        return false;
    }
    return sendFloatCommand(static_cast<int>(SerialCommand::CMD_CONNECT_MOTOR), 0.12);
}

/**
 * @brief 发送原始文本数据（UTF-8 编码）
 *
 * 注意：此方法发送的是原始文本，不包含协议帧头和校验。
 * 通常用于调试目的，正式的参数交互应使用 sendFloatCommand。
 *
 * @param text 要发送的文本
 * @return true 发送成功
 */
bool SerialManager::sendText(const QString &text)
{
#if !LIJOINT_HAS_SERIALPORT
    Q_UNUSED(text);
    setStatusMessage(QStringLiteral("发送失败: 当前 Qt 环境缺少 SerialPort 模块"));
    return false;
#else
    if (!isConnected()) {
        setStatusMessage(QStringLiteral("发送失败: 串口未连接"));
        return false;
    }

    const QByteArray data = text.toUtf8();
    const qint64 written = m_serialPort->write(data);
    if (written != data.size()) {
        setStatusMessage(QStringLiteral("发送失败: %1").arg(m_serialPort->errorString()));
        return false;
    }

    setStatusMessage(QStringLiteral("已发送 %1 字节").arg(written));
    return true;
#endif
}

/**
 * @brief 发送浮点命令帧（核心发送接口）
 *
 * 构造固定 8 字节的命令帧并写入串口：
 *   [0xA5][CMD][4字节float LE][CHECKSUM][0x49]
 *
 * 校验和 = 前 6 字节累加和。
 *
 * @param command 命令字（0-255）
 * @param value   伴随的 float 参数值
 * @return true 发送成功
 */
bool SerialManager::sendFloatCommand(int command, double value)
{
#if !LIJOINT_HAS_SERIALPORT
    Q_UNUSED(command);
    Q_UNUSED(value);
    setStatusMessage(QStringLiteral("发送失败: 当前 Qt 环境缺少 SerialPort 模块"));
    return false;
#else
    if (!isConnected()) {
        setStatusMessage(QStringLiteral("发送失败: 串口未连接"));
        return false;
    }

    if (command < 0 || command > 0xFF) {
        setStatusMessage(QStringLiteral("发送失败: 命令字非法"));
        return false;
    }

    const auto cmd = static_cast<uint8_t>(command);
    const float floatValue = static_cast<float>(value);

    // 将 float 按小端序拆分为 4 字节
    uint8_t payload[4] = {0};
    std::memcpy(payload, &floatValue, sizeof(float));

    // 构造帧：[HEAD][CMD][DATA0~3][CHECKSUM][TAIL]
    QByteArray frame;
    frame.reserve(8);
    frame.append(static_cast<char>(FRAME_HEAD));
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(payload[0]));
    frame.append(static_cast<char>(payload[1]));
    frame.append(static_cast<char>(payload[2]));
    frame.append(static_cast<char>(payload[3]));

    // 计算校验和（前 6 字节累加）
    uint8_t checksum = 0;
    for (int i = 0; i < 6; ++i) {
        checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(frame[i]));
    }

    frame.append(static_cast<char>(checksum));
    frame.append(static_cast<char>(FRAME_TAIL));

    const qint64 written = m_serialPort->write(frame);
    if (written != frame.size()) {
        setStatusMessage(QStringLiteral("发送失败: %1").arg(m_serialPort->errorString()));
        return false;
    }

    setStatusMessage(QStringLiteral("已发送命令 0x%1").arg(command, 2, 16, QLatin1Char('0')).toUpper());
    return true;
#endif
}

void SerialManager::playSystemAlert()
{
#ifdef Q_OS_WIN
    MessageBeep(MB_OK);
#else
    // Fallback terminal bell for non-Windows platforms.
    fputs("\a", stdout);
    fflush(stdout);
#endif
}

/**
 * @brief 启用/禁用串口接收日志
 *
 * 接收日志默认禁用，因为高频串口数据会触发大量信号，
 * 导致 UI 线程中 QTextEdit::append 过于频繁，影响图表刷新。
 *
 * @param enabled true 启用日志（会发射 dataReceived 信号）
 */
void SerialManager::setRxLogEnabled(bool enabled)
{
    m_rxLogEnabled = enabled;
}

/* ---------- 私有状态设置方法（含去重逻辑，避免重复发射信号） ---------- */

void SerialManager::setMotorConnected(bool connected)
{
    if (m_motorConnected == connected) {
        return;
    }
    m_motorConnected = connected;
    emit motorConnectedChanged();
}

void SerialManager::setMosTemperature(double temperature)
{
    if (qFuzzyCompare(m_mosTemperature + 1.0, temperature + 1.0)) {
        return;
    }
    m_mosTemperature = temperature;
    emit mosTemperatureChanged();
}

void SerialManager::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}

void SerialManager::setActiveTrendCommand(int command)
{
    if (m_activeTrendCommand == command) {
        return;
    }
    m_activeTrendCommand = command;
    emit activeTrendCommandChanged();
}

/**
 * @brief QSerialPort::readyRead 处理入口
 *
 * 读取串口所有可用字节，拼接到接收缓冲区，然后调用帧解析。
 * 若启用日志，同时发射 HEX 格式数据以供 UI 显示。
 */
void SerialManager::readSerialData()
{
#if !LIJOINT_HAS_SERIALPORT
    return;
#else
    const QByteArray bytes = m_serialPort->readAll();
    if (bytes.isEmpty()) {
        return;
    }

    if (m_rxLogEnabled) {
        emit dataReceived(QStringLiteral("RX HEX: %1").arg(QString::fromLatin1(bytes.toHex(' ')).toUpper()));
    }

    m_rxBuffer.append(bytes);
    parseRxBuffer();
#endif
}

/**
 * @brief 接收缓冲区帧解析主循环
 *
 * 循环调用 tryParseOneFrame 从缓冲区头部解析完整帧，
 * 每解析完一帧就移除已消耗的字节。
 */
void SerialManager::parseRxBuffer()
{
    while (!m_rxBuffer.isEmpty()) {
        int consumedBytes = 0;
        if (!tryParseOneFrame(consumedBytes)) {
            return;  // 数据不足，等更多数据
        }

        if (consumedBytes <= 0 || consumedBytes > m_rxBuffer.size()) {
            m_rxBuffer.clear();  // 保护性清空
            return;
        }

        m_rxBuffer.remove(0, consumedBytes);
    }
}

/**
 * @brief 尝试从接收缓冲区头部解析一个完整的协议帧
 *
 * 接收帧格式：
 *   [0xA5][CMD][LEN][PAYLOAD...][CHECKSUM][0x49]
 *
 * 解析流程：
 * 1. 查找帧头 0xA5，未找到则丢弃所有数据
 * 2. 帧头不在缓冲区头部时，丢弃前置字节
 * 3. 检查 LEN 合法性（>0, ≤128, 4的倍数）
 * 4. 等待完整帧到达
 * 5. 验证帧尾 0x49
 * 6. 验证校验和
 * 7. 提取载荷并解析为 float 数组
 * 8. 发射 frameParsed 信号，更新电机/温度/趋势状态
 *
 * @param[out] consumedBytes 本帧消耗的字节数
 * @return true  成功解析一帧（或跳过无用字节）
 * @return false 数据不足，需要等待更多数据
 */
bool SerialManager::tryParseOneFrame(int &consumedBytes)
{
    consumedBytes = 0;

    // 最小帧：[HEAD][CMD][LEN][CHECKSUM][TAIL] = 5 字节
    if (m_rxBuffer.size() < 5) {
        return false;
    }

    // 查找帧头 0xA5
    const int headIndex = m_rxBuffer.indexOf(static_cast<char>(FRAME_HEAD));
    if (headIndex < 0) {
        consumedBytes = m_rxBuffer.size() - 1;  // 保留最后 1 字节，避免跨两包的 0xA5
        return true;
    }

    if (headIndex > 0) {
        consumedBytes = headIndex;  // 丢弃帧头前的垃圾字节
        return true;
    }

    // 解析帧头后的字段
    const uint8_t cmd = static_cast<uint8_t>(m_rxBuffer[1]);  // 命令字
    const uint8_t len = static_cast<uint8_t>(m_rxBuffer[2]);  // 载荷长度

    // 校验 LEN：必须 >0、≤128、且为 4 的倍数（float 数组对齐）
    if (len == 0 || len > kMaxPayloadLen || (len % 4) != 0) {
        consumedBytes = 1;
        return true;
    }

    const int frameSize = 5 + len;  // 完整帧总长度
    if (m_rxBuffer.size() < frameSize) {
        return false;  // 数据不足，等待更多
    }

    // 验证帧尾
    if (static_cast<uint8_t>(m_rxBuffer[frameSize - 1]) != FRAME_TAIL) {
        consumedBytes = 1;
        return true;
    }

    // 校验和计算（所有字节除了校验和自身和帧尾）
    uint8_t checksum = 0;
    for (int i = 0; i < frameSize - 2; ++i) {
        checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(m_rxBuffer[i]));
    }

    if (checksum != static_cast<uint8_t>(m_rxBuffer[frameSize - 2])) {
        consumedBytes = 1;  // 校验失败，按字节滑动重同步
        return true;
    }

    // 提取载荷并解析为 QVariantList（float 数组）
    const QByteArray payload = m_rxBuffer.mid(3, len);
    const int count = len / 4;

    QVariantList values;
    values.reserve(count);
    for (int i = 0; i < count; ++i) {
        values.push_back(bytesToFloat(payload, i * 4));
    }

    // 发射解析完成信号供 UI 层消费
    emit frameParsed(static_cast<int>(cmd), values);

    // ---- 状态维护 ----
    // 连接电机成功
    if (cmd == static_cast<uint8_t>(SerialCommand::CMD_CONNECT_MOTOR)) {
        setMotorConnected(true);
        setStatusMessage(QStringLiteral("电机连接成功"));
    }

    // 零位校准完成
    if (cmd == static_cast<uint8_t>(SerialCommand::CMD_ZEROCALIBRATIO_OVER)) {
        setStatusMessage(QStringLiteral("零电位校准完成"));
    }

    // 更新 MOS 温度独立属性
    if (cmd == static_cast<uint8_t>(SerialCommand::CMD_MOSTEMP) && !values.isEmpty()) {
        setMosTemperature(values[0].toDouble());
    }

    // 除 MOS 温度外的采样数据，发射简化的趋势信号（UI 波形更新用）
    if (!values.isEmpty()) {
        const double a = values[0].toDouble();
        const double b = values.size() > 1 ? values[1].toDouble() : 0.0;
        if (cmd != static_cast<uint8_t>(SerialCommand::CMD_MOSTEMP)) {
            emit trendDataReceived(a, b, static_cast<int>(cmd));
        }
    }

    consumedBytes = frameSize;
    return true;
}

/**
 * @brief 将缓冲区中的 4 个字节解析为 float（小端序）
 *
 * 使用 std::memcpy 避免未定义对齐访问。
 *
 * @param data  字节数组
 * @param index 起始偏移
 * @return 解析出的 float 值
 */
float SerialManager::bytesToFloat(const QByteArray &data, int index)
{
    if (data.size() < index + 4) {
        return 0.0F;
    }

    float value = 0.0F;
    std::memcpy(&value, data.constData() + index, sizeof(float));
    return value;
}
