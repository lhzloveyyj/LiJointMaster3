#pragma once

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

    QStringList availablePorts() const;
    bool isConnected() const;
    bool isMotorConnected() const;
    QString statusMessage() const;
    double mosTemperature() const;
    int activeTrendCommand() const;

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE bool connectPort(const QString &portName, int baudRate);
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE bool connectMotor();
    Q_INVOKABLE bool sendText(const QString &text);
    Q_INVOKABLE bool sendFloatCommand(int command, double value);
    Q_INVOKABLE void playSystemAlert();
    Q_INVOKABLE void setRxLogEnabled(bool enabled);

signals:
    void availablePortsChanged();
    void connectedChanged();
    void motorConnectedChanged();
    void statusMessageChanged();
    void mosTemperatureChanged();
    void activeTrendCommandChanged();
    void dataReceived(const QString &text);
    void frameParsed(int command, QVariantList values);
    void trendDataReceived(double a, double b, int command);

private:
    void updateAvailablePorts(bool manualRefresh);
    void setStatusMessage(const QString &message);
    void readSerialData();
    void parseRxBuffer();
    bool tryParseOneFrame(int &consumedBytes);
    static float bytesToFloat(const QByteArray &data, int index);
    void setMotorConnected(bool connected);
    void setMosTemperature(double temperature);
    void setActiveTrendCommand(int command);

#if LIJOINT_HAS_SERIALPORT
    QSerialPort *m_serialPort;
#endif
    QStringList m_availablePorts;
    bool m_motorConnected;
    QString m_statusMessage;
    double m_mosTemperature;
    int m_activeTrendCommand;
    QByteArray m_rxBuffer;
    QTimer *m_portWatchTimer;
    bool m_rxLogEnabled;
};
