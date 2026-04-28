#pragma once

/**
 * @file serialmanager.h
 * @brief 串口管理器类声明
 *
 * 负责串口设备的枚举、连接/断开、数据收发、协议帧解析，
 * 以及电机连接状态、MOS 温度、活动波形命令等运行状态的维护。
 *
 * 通过 QSerialPort 实现底层通信（编译时可选的模块），
 * 若无 SerialPort 模块则串口功能静默禁用。
 */

#include <QObject>
#include <QStringList>
#include <QByteArray>
#include <QVariantList>

#ifndef LIJOINT_HAS_SERIALPORT
#define LIJOINT_HAS_SERIALPORT 0
#endif

class QTimer;

#if LIJOINT_HAS_SERIALPORT
class QSerialPort;
#endif

/**
 * @brief 串口管理器
 *
 * 核心职责：
 * 1. 串口热插拔检测（300ms 定时器轮询）
 * 2. 串口打开/关闭（固定 8N1 无流控）
 * 3. 固定 8 字节命令帧发送
 * 4. 可变长度回包帧解析（含帧同步、校验和验证）
 * 5. 电机连接状态、MOS 温度、活动波形的属性管理
 *
 * 所有状态变化通过 Qt 信号/属性通知 UI 层更新。
 */
class SerialManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY availablePortsChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool motorConnected READ isMotorConnected NOTIFY motorConnectedChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(double mosTemperature READ mosTemperature NOTIFY mosTemperatureChanged)
    Q_PROPERTY(int activeTrendCommand READ activeTrendCommand WRITE setActiveTrendCommand NOTIFY activeTrendCommandChanged)

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager() override;

    /* ---------- 属性读取 ---------- */
    QStringList availablePorts() const;     ///< 当前可用串口列表
    bool isConnected() const;               ///< 串口是否已打开
    bool isMotorConnected() const;          ///< 电机是否已连接
    QString statusMessage() const;          ///< 当前状态文本
    double mosTemperature() const;          ///< MOS 管温度（℃）
    int activeTrendCommand() const;         ///< 当前活动的波形命令字

    /* ---------- QML/Q_INVOKABLE 公开接口 ---------- */
    Q_INVOKABLE void refreshPorts();                        ///< 手动刷新串口列表
    Q_INVOKABLE bool connectPort(const QString &portName, int baudRate);  ///< 打开指定串口
    Q_INVOKABLE void disconnectPort();                      ///< 断开串口连接
    Q_INVOKABLE bool connectMotor();                        ///< 发送电机连接命令
    Q_INVOKABLE bool sendText(const QString &text);         ///< 发送原始文本（UTF-8）
    Q_INVOKABLE bool sendFloatCommand(int command, double value);  ///< 发送 float 命令帧
    Q_INVOKABLE void playSystemAlert();                     ///< 播放系统提示音
    Q_INVOKABLE void setRxLogEnabled(bool enabled);         ///< 启用/禁用接收日志

signals:
    /* ---------- 属性变化通知 ---------- */
    void availablePortsChanged();           ///< 可用串口列表变化
    void connectedChanged();                ///< 串口连接状态变化
    void motorConnectedChanged();           ///< 电机连接状态变化
    void statusMessageChanged();            ///< 状态文本变化
    void mosTemperatureChanged();           ///< MOS 温度变化
    void activeTrendCommandChanged();       ///< 活动波形命令变化
    void dataReceived(const QString &text); ///< 原始接收数据（HEX 格式日志）
    void frameParsed(int command, QVariantList values);  ///< 解析完成的协议帧
    void trendDataReceived(double a, double b, int command);  ///< 趋势图数据（简化版）

private:
    /* ---------- 内部方法 ---------- */
    void updateAvailablePorts(bool manualRefresh);  ///< 更新可用串口列表
    void setStatusMessage(const QString &message);  ///< 设置状态文本（去重）
    void readSerialData();                          ///< readyRead 处理入口
    void parseRxBuffer();                           ///< 循环解析接收缓冲区
    bool tryParseOneFrame(int &consumedBytes);      ///< 尝试解析一个完整帧
    static float bytesToFloat(const QByteArray &data, int index);  ///< 4 字节转 float
    void setMotorConnected(bool connected);         ///< 设置电机连接状态
    void setMosTemperature(double temperature);     ///< 设置 MOS 温度
    void setActiveTrendCommand(int command);        ///< 设置活动波形命令

    /* ---------- 成员变量 ---------- */
#if LIJOINT_HAS_SERIALPORT
    QSerialPort *m_serialPort;          ///< Qt 串口对象
#endif
    QStringList m_availablePorts;       ///< 可用串口列表缓存
    bool m_motorConnected;              ///< 电机连接状态
    QString m_statusMessage;            ///< 当前状态文本
    double m_mosTemperature;            ///< MOS 管温度
    int m_activeTrendCommand;           ///< 当前活动的波形订阅命令字
    QByteArray m_rxBuffer;             ///< 接收数据缓冲区（粘包重组）
    QTimer *m_portWatchTimer;          ///< 串口热插拔轮询定时器
    bool m_rxLogEnabled;               ///< 接收日志启用标志
};
