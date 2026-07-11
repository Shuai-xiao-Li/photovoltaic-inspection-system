/**
 * @file vehicledata.h
 * @brief 底盘实时状态与控制接口类头文件，声明物理串口参数、接收缓冲区以及控制函数。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef VEHICLEDATA_H
#define VEHICLEDATA_H

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QSerialPort>
#include <QByteArray>

class VehicleData : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double speed READ speed NOTIFY dataChanged)
    Q_PROPERTY(double batteryVoltage READ batteryVoltage NOTIFY dataChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY dataChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY dataChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY dataChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY dataChanged)
    Q_PROPERTY(int faultCode READ faultCode NOTIFY dataChanged)
    Q_PROPERTY(bool serialConnected READ serialConnected NOTIFY connectionChanged)
    Q_PROPERTY(int fps READ fps NOTIFY dataChanged)
    Q_PROPERTY(QString uptimeStr READ uptimeStr NOTIFY dataChanged)
    
    // New UI Properties
    Q_PROPERTY(int currentMode READ currentMode WRITE setCurrentMode NOTIFY modeChanged)
    Q_PROPERTY(int motorStatus READ motorStatus NOTIFY motorChanged)
    Q_PROPERTY(QStringList systemLogs READ systemLogs NOTIFY logsChanged)

    // Task Progress Properties
    Q_PROPERTY(QString currentTaskName READ currentTaskName NOTIFY taskChanged)
    Q_PROPERTY(int inspectedPanels READ inspectedPanels NOTIFY taskChanged)
    Q_PROPERTY(int totalPanels READ totalPanels NOTIFY taskChanged)
    Q_PROPERTY(QString nextTarget READ nextTarget NOTIFY taskChanged)
    Q_PROPERTY(int etaSeconds READ etaSeconds NOTIFY taskChanged)

public:
    explicit VehicleData(QObject *parent = nullptr);

    double speed() const { return m_speed; }
    double batteryVoltage() const { return m_batteryVoltage; }
    int batteryPercent() const { return m_batteryPercent; }
    double temperature() const { return m_temperature; }
    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    int faultCode() const { return m_faultCode; }
    bool serialConnected() const { return m_serialConnected; }
    int fps() const { return m_fps; }
    QString uptimeStr() const;

    int currentMode() const { return m_currentMode; }
    void setCurrentMode(int mode);

    int motorStatus() const { return m_motorStatus; }
    QStringList systemLogs() const { return m_systemLogs; }

    QString currentTaskName() const { return m_currentTaskName; }
    int inspectedPanels() const { return m_inspectedPanels; }
    int totalPanels() const { return m_totalPanels; }
    QString nextTarget() const { return m_nextTarget; }
    int etaSeconds() const { return m_etaSeconds; }

    Q_INVOKABLE void addLog(const QString &msg);
    Q_INVOKABLE void sendControlCommand(int commandId, int value = 0);
    
    // API to sync statistics from AI detection
    void updateInspectionStats(int inspected, int total);

signals:
    void dataChanged();
    void connectionChanged();
    void modeChanged();
    void motorChanged();
    void logsChanged();
    void taskChanged();

private slots:
    void initSerialPort();
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void updateUptimeData();

private:
    QTimer m_timer;
    QSerialPort *m_serialPort = nullptr;
    QByteArray m_serialBuffer;

    double m_speed = 0.0;
    double m_batteryVoltage = 0.0;
    int m_batteryPercent = 0;
    double m_temperature = 0.0;
    double m_latitude = 39.9042;
    double m_longitude = 116.4074;
    int m_faultCode = 0;
    bool m_serialConnected = false;
    int m_fps = 0;
    int m_uptimeSeconds = 0;
    int m_tickCount = 0;

    int m_currentMode = 0; // 0: Auto, 1: Manual
    int m_motorStatus = 0; // 0: Normal

    QStringList m_systemLogs;

    QString m_currentTaskName = "沙戈荒 A 区阵列特巡";
    int m_inspectedPanels = 0;
    int m_totalPanels = 120;
    QString m_nextTarget = "B-14 组节点";
    int m_etaSeconds = 840; // 14 mins
};

#endif // VEHICLEDATA_H
