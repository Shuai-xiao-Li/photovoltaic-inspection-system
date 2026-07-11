/**
 * @file vehicledata.cpp
 * @brief 底盘实时状态与控制接口实现，负责真实物理串口的开启、自动重连、二进制协议帧反序列化、物理量单位转换（如速度与电压映射），以及发送控制小车的串口指令。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "vehicledata.h"
#include <QtMath>
#include <QRandomGenerator>
#include <QDateTime>

#include <QFile>

VehicleData::VehicleData(QObject *parent)
    : QObject(parent)
{
    addLog("系统启动，初始化进行中");
    
    // 初始化并开启物理串口连接
    initSerialPort();

    // 启动系统运行计时与地图微量漂移定时器 (1Hz 周期)
    connect(&m_timer, &QTimer::timeout, this, &VehicleData::updateUptimeData);
    m_timer.start(1000);
}

void VehicleData::setCurrentMode(int mode)
{
    if (m_currentMode != mode) {
        m_currentMode = mode;
        emit modeChanged();
        addLog(mode == 0 ? "已切回自动巡检模式" : "已切换为手动控制模式");
    }
}

void VehicleData::addLog(const QString &msg)
{
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_systemLogs.append(QString("[%1] %2").arg(timeStr, msg));
    if (m_systemLogs.size() > 50) {
        m_systemLogs.removeFirst();
    }
    emit logsChanged();
}

void VehicleData::sendControlCommand(int commandId, int value)
{
    // commandId: 1: Forward, 2: Backward, 3: Left, 4: Right, 5: Stop, 6: Speed
    QString cmdStr;
    switch (commandId) {
        case 1: cmdStr = "vel 150 0\r\n"; break;   // 前进：150mm/s
        case 2: cmdStr = "vel -150 0\r\n"; break;  // 后退：-150mm/s
        case 3: cmdStr = "vel 0 20\r\n"; break;    // 左转
        case 4: cmdStr = "vel 0 -20\r\n"; break;   // 右转
        case 5: cmdStr = "vel 0 0\r\nstop\r\n"; break; // 刹车急停
        case 6: cmdStr = QString("vel %1 0\r\n").arg(value); break; // 调速
        default: return;
    }
    
    if (m_serialPort && m_serialConnected && m_serialPort->isOpen()) {
        m_serialPort->write(cmdStr.toUtf8());
        addLog(QString("下发串口控制指令: %1").arg(cmdStr.trimmed()));
    } else {
        addLog(QString("指令发送失败，串口未开启: %1").arg(cmdStr.trimmed()));
    }
}

void VehicleData::initSerialPort()
{
    if (m_serialPort) {
        m_serialPort->close();
        delete m_serialPort;
        m_serialPort = nullptr;
    }

    m_serialPort = new QSerialPort(this);
    
    // 飞腾派首选USB转串口设备 /dev/ttyUSB0，若不存在则使用板载引脚 /dev/ttyS1
    QString portName = "/dev/ttyUSB0";
    if (!QFile::exists(portName)) {
        portName = "/dev/ttyS1";
    }
    
    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    
    connect(m_serialPort, &QSerialPort::readyRead, this, &VehicleData::onReadyRead);
    connect(m_serialPort, static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)>(&QSerialPort::error),
            this, &VehicleData::onSerialError);
            
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_serialConnected = true;
        addLog(QString("物理串口 %1 开启成功 (115200, 8N1)").arg(portName));
    } else {
        m_serialConnected = false;
        addLog(QString("物理串口 %1 开启失败: %2 (3秒后重连)").arg(portName, m_serialPort->errorString()));
        QTimer::singleShot(3000, this, &VehicleData::initSerialPort);
    }
    emit connectionChanged();
}

void VehicleData::onReadyRead()
{
    m_serialBuffer.append(m_serialPort->readAll());
    
    // 协议帧大小为 10 字节
    // 格式: 0xAA 0x55 [Length=6] [SpeedL] [SpeedH] [VoltL] [VoltH] [Temp] [Fault] [Checksum]
    while (m_serialBuffer.size() >= 10) {
        if (static_cast<uint8_t>(m_serialBuffer[0]) != 0xAA || static_cast<uint8_t>(m_serialBuffer[1]) != 0x55) {
            m_serialBuffer.remove(0, 1);
            continue;
        }
        
        if (static_cast<uint8_t>(m_serialBuffer[2]) != 6) {
            m_serialBuffer.remove(0, 3);
            continue;
        }
        
        uint8_t checksum = 0;
        for (int i = 3; i < 9; ++i) {
            checksum ^= static_cast<uint8_t>(m_serialBuffer[i]);
        }
        
        if (checksum != static_cast<uint8_t>(m_serialBuffer[9])) {
            m_serialBuffer.remove(0, 2);
            continue;
        }
        
        // 解析各物理域并高低位拼接
        int16_t rawSpeed = static_cast<int16_t>((static_cast<uint8_t>(m_serialBuffer[4]) << 8) | static_cast<uint8_t>(m_serialBuffer[3]));
        uint16_t rawVolt = static_cast<uint16_t>((static_cast<uint8_t>(m_serialBuffer[6]) << 8) | static_cast<uint8_t>(m_serialBuffer[5]));
        int8_t rawTemp = static_cast<int8_t>(m_serialBuffer[7]);
        uint8_t rawFault = static_cast<uint8_t>(m_serialBuffer[8]);
        
        // 转化单位：mm/s -> km/h (底盘速度物理量化值)
        m_speed = qAbs(rawSpeed) * 0.0036; 
        
        // mV -> V
        m_batteryVoltage = rawVolt / 1000.0;
        
        // 电池电量映射 (锂电池3S: 9.9V - 12.6V)
        m_batteryPercent = qBound(0, static_cast<int>((m_batteryVoltage - 9.9) / (12.6 - 9.9) * 100.0), 100);
        
        m_temperature = rawTemp;
        m_faultCode = rawFault;
        
        m_serialBuffer.remove(0, 10);
        emit dataChanged();
    }
}

void VehicleData::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        addLog("警告: 物理串口断开，连接被移除！");
        m_serialConnected = false;
        emit connectionChanged();
        if (m_serialPort) {
            m_serialPort->close();
        }
        QTimer::singleShot(3000, this, &VehicleData::initSerialPort);
    }
}

void VehicleData::updateUptimeData()
{
    m_uptimeSeconds++;
    m_tickCount++;
    double t = m_tickCount;
    
    // GPS 微幅漂移（保持地图界面动感）
    m_latitude  = 39.9042 + qSin(t * 0.02) * 0.0003;
    m_longitude = 116.4074 + qCos(t * 0.016) * 0.0003;
    
    // 刷新 FPS 指示
    m_fps = m_serialConnected ? (29 + QRandomGenerator::global()->bounded(-1, 2)) : 0;
    
    emit dataChanged();
}

void VehicleData::updateInspectionStats(int inspected, int total)
{
    if (m_inspectedPanels != inspected || m_totalPanels != total) {
        m_inspectedPanels = inspected;
        m_totalPanels = total;
        m_etaSeconds = qMax(0, 840 - inspected * 7); // 根据检查进度自缩减 ETA 估算
        emit taskChanged();
    }
}

QString VehicleData::uptimeStr() const
{
    int h = m_uptimeSeconds / 3600;
    int m = (m_uptimeSeconds % 3600) / 60;
    int s = m_uptimeSeconds % 60;
    return QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
}
