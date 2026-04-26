#include "serialmanager.h"

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
// 回包载荷保护上限，防止异常帧撑爆解析逻辑
constexpr int kMaxPayloadLen = 128;

QString normalizedPortName(const QString &portName)
{
    if (portName.startsWith(QStringLiteral("/dev/"))) {
        return QFileInfo(portName).fileName();
    }
    return portName;
}

bool containsPort(const QStringList &ports, const QString &portName)
{
    const QString normalized = normalizedPortName(portName);
    return ports.contains(portName)
        || ports.contains(normalized)
        || ports.contains(QStringLiteral("/dev/") + normalized);
}
}

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
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialManager::readSerialData);

    connect(m_serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) {
            return;
        }
        setStatusMessage(QStringLiteral("串口错误: %1").arg(m_serialPort->errorString()));
    });
#endif

    connect(m_portWatchTimer, &QTimer::timeout, this, [this]() {
        updateAvailablePorts(false);
    });
    m_portWatchTimer->setInterval(300);
    m_portWatchTimer->start();

    updateAvailablePorts(true);
}

SerialManager::~SerialManager()
{
    disconnectPort();
}

QStringList SerialManager::availablePorts() const
{
    return m_availablePorts;
}

bool SerialManager::isConnected() const
{
#if LIJOINT_HAS_SERIALPORT
    return m_serialPort->isOpen();
#else
    return false;
#endif
}

bool SerialManager::isMotorConnected() const
{
    return m_motorConnected;
}

QString SerialManager::statusMessage() const
{
    return m_statusMessage;
}

double SerialManager::mosTemperature() const
{
    return m_mosTemperature;
}

int SerialManager::activeTrendCommand() const
{
    return m_activeTrendCommand;
}

void SerialManager::refreshPorts()
{
    updateAvailablePorts(true);
}

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
    // 轮询当前系统串口列表
    QStringList preferredPorts;
    QStringList otherPorts;
    const auto portInfos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : portInfos) {
        const QString location = info.systemLocation().isEmpty() ? info.portName() : info.systemLocation();
        if (location.contains(QStringLiteral("ttyUSB")) || location.contains(QStringLiteral("ttyACM"))) {
            preferredPorts << location;
        } else {
            otherPorts << location;
        }
    }

#ifdef Q_OS_LINUX
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

    if (isConnected() && !containsPort(ports, m_serialPort->portName())) {
        // 已连接串口被拔出：执行自动断连与状态复位
        const QString removedPort = m_serialPort->portName();
        m_serialPort->close();
        m_rxBuffer.clear();
        setMotorConnected(false);
        setMosTemperature(0.0);
        setActiveTrendCommand(0);
        emit connectedChanged();
        setStatusMessage(QStringLiteral("检测到串口已移除: %1").arg(removedPort));
    }

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

bool SerialManager::connectMotor()
{
    if (!isConnected()) {
        setStatusMessage(QStringLiteral("连接电机失败: 串口未连接"));
        return false;
    }
    return sendFloatCommand(static_cast<int>(SerialCommand::CMD_CONNECT_MOTOR), 0.12);
}

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

bool SerialManager::sendFloatCommand(int command, double value)
{
#if !LIJOINT_HAS_SERIALPORT
    Q_UNUSED(command);
    Q_UNUSED(value);
    setStatusMessage(QStringLiteral("发送失败: 当前 Qt 环境缺少 SerialPort 模块"));
    return false;
#else
    // 命令帧固定 8 字节：
    // [HEAD][CMD][DATA0][DATA1][DATA2][DATA3][CHECKSUM][TAIL]
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

    uint8_t payload[4] = {0};
    std::memcpy(payload, &floatValue, sizeof(float));

    // MCU 侧接收解析为固定 8 字节：
    // [HEAD][CMD][DATA0][DATA1][DATA2][DATA3][CHECKSUM][TAIL]
    QByteArray frame;
    frame.reserve(8);
    frame.append(static_cast<char>(FRAME_HEAD));
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(payload[0]));
    frame.append(static_cast<char>(payload[1]));
    frame.append(static_cast<char>(payload[2]));
    frame.append(static_cast<char>(payload[3]));

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

void SerialManager::setRxLogEnabled(bool enabled)
{
    m_rxLogEnabled = enabled;
}

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

void SerialManager::readSerialData()
{
#if !LIJOINT_HAS_SERIALPORT
    return;
#else
    // readyRead 时先收原始字节，再做协议帧解析
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

void SerialManager::parseRxBuffer()
{
    while (!m_rxBuffer.isEmpty()) {
        int consumedBytes = 0;
        if (!tryParseOneFrame(consumedBytes)) {
            return;
        }

        if (consumedBytes <= 0 || consumedBytes > m_rxBuffer.size()) {
            m_rxBuffer.clear();
            return;
        }

        m_rxBuffer.remove(0, consumedBytes);
    }
}

bool SerialManager::tryParseOneFrame(int &consumedBytes)
{
    // 帧格式：
    // [HEAD][CMD][LEN][PAYLOAD...][CHECKSUM][TAIL]
    consumedBytes = 0;

    if (m_rxBuffer.size() < 5) {
        return false;
    }

    const int headIndex = m_rxBuffer.indexOf(static_cast<char>(FRAME_HEAD));
    if (headIndex < 0) {
        consumedBytes = m_rxBuffer.size() - 1;
        return true;
    }

    if (headIndex > 0) {
        consumedBytes = headIndex;
        return true;
    }

    const uint8_t cmd = static_cast<uint8_t>(m_rxBuffer[1]);
    const uint8_t len = static_cast<uint8_t>(m_rxBuffer[2]);

    if (len == 0 || len > kMaxPayloadLen || (len % 4) != 0) {
        // LEN 必须为 4 的倍数（float 数组）
        consumedBytes = 1;
        return true;
    }

    const int frameSize = 5 + len;
    if (m_rxBuffer.size() < frameSize) {
        return false;
    }

    if (static_cast<uint8_t>(m_rxBuffer[frameSize - 1]) != FRAME_TAIL) {
        consumedBytes = 1;
        return true;
    }

    uint8_t checksum = 0;
    for (int i = 0; i < frameSize - 2; ++i) {
        checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(m_rxBuffer[i]));
    }

    if (checksum != static_cast<uint8_t>(m_rxBuffer[frameSize - 2])) {
        // 校验失败：丢弃一个字节继续同步
        consumedBytes = 1;
        return true;
    }

    const QByteArray payload = m_rxBuffer.mid(3, len);
    const int count = len / 4;

    QVariantList values;
    values.reserve(count);
    for (int i = 0; i < count; ++i) {
        values.push_back(bytesToFloat(payload, i * 4));
    }

    emit frameParsed(static_cast<int>(cmd), values);

    // 连接电机成功状态维护
    if (cmd == static_cast<uint8_t>(SerialCommand::CMD_CONNECT_MOTOR)) {
        setMotorConnected(true);
        setStatusMessage(QStringLiteral("电机连接成功"));
    }

    if (cmd == static_cast<uint8_t>(SerialCommand::CMD_ZEROCALIBRATIO_OVER)) {
        setStatusMessage(QStringLiteral("零电位校准完成"));
    }

    // MOS 温度独立属性更新
    if (cmd == static_cast<uint8_t>(SerialCommand::CMD_MOSTEMP) && !values.isEmpty()) {
        setMosTemperature(values[0].toDouble());
    }

    // 除 MOS 外的采样统一进入趋势数据信号
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

float SerialManager::bytesToFloat(const QByteArray &data, int index)
{
    if (data.size() < index + 4) {
        return 0.0F;
    }

    float value = 0.0F;
    std::memcpy(&value, data.constData() + index, sizeof(float));
    return value;
}
