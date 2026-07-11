/**
 * @file detectionbridge.cpp
 * @brief Python AI 算法与 C++ 界面层通信桥梁类实现，负责轮询解析 /tmp/phytium_detection.json 结果以及刷新 /tmp/phytium_frame.bmp 标注图像。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "detectionbridge.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDebug>

const QString DetectionBridge::JSON_PATH = "/tmp/phytium_detection.json";
const QString DetectionBridge::FRAME_PATH = "/tmp/phytium_frame.bmp";

DetectionBridge::DetectionBridge(QObject *parent)
    : QObject(parent)
{
    // 轮询 JSON 检测结果（200ms 间隔，平衡性能与响应）
    connect(&m_jsonTimer, &QTimer::timeout, this, &DetectionBridge::pollDetectionData);
    m_jsonTimer.start(200);

    // 轮询帧图片（15ms 极低延迟，约 66Hz 刷新率）
    connect(&m_frameTimer, &QTimer::timeout, this, &DetectionBridge::pollFrameImage);
    m_frameTimer.start(15);
}

void DetectionBridge::pollDetectionData()
{
    QFileInfo fi(JSON_PATH);

    // ── 文件不存在 → 算法离线 ──
    if (!fi.exists()) {
        if (m_algorithmOnline) {
            m_algorithmOnline = false;
            m_className = "";
            m_confidence = 0.0;
            m_cameraOk = false;
            emit detectionUpdated();
        }
        return;
    }

    qint64 lastMod = fi.lastModified().toMSecsSinceEpoch();
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // ── 算法在线判断：JSON 文件在 3 秒内有更新 ──
    bool online = (now - lastMod) < 3000;

    // ── 文件未变化，仅更新在线状态 ──
    if (lastMod == m_lastJsonTimestamp) {
        if (m_algorithmOnline != online) {
            m_algorithmOnline = online;
            emit detectionUpdated();
        }
        return;
    }
    m_lastJsonTimestamp = lastMod;

    // ── 读取并解析 JSON ──
    QFile file(JSON_PATH);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "[DetectionBridge] JSON parse error:" << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();

    // ── 更新所有属性 ──
    m_className      = obj.value("class_name").toString();
    m_confidence     = obj.value("confidence").toDouble();
    m_centerX        = obj.value("center_x").toInt();
    m_centerY        = obj.value("center_y").toInt();
    m_flag           = obj.value("flag").toInt();
    m_inferTimeMs    = obj.value("infer_time_ms").toDouble();
    m_cameraOk       = obj.value("camera_ok").toBool();
    m_detectionCount = obj.value("detection_count").toInt();
    m_occlusionCount = obj.value("occlusion_count").toInt();
    m_damageCount    = obj.value("damage_count").toInt();
    m_detectionFps   = obj.value("fps").toDouble();
    m_uptimeSeconds  = obj.value("uptime_seconds").toInt();
    m_frameCount     = obj.value("frame_count").toInt();
    m_algorithmOnline = online;

    emit detectionUpdated();
}

void DetectionBridge::pollFrameImage()
{
    QFileInfo fi(FRAME_PATH);
    if (!fi.exists()) {
        // 帧文件不存在，清空 URL（QML 将显示占位图）
        if (!m_frameImageUrl.isEmpty()) {
            m_frameImageUrl = "";
            emit frameUpdated();
        }
        return;
    }

    qint64 lastMod = fi.lastModified().toMSecsSinceEpoch();

    // ── 文件有更新 → 递增计数器，强制 QML Image 刷新 ──
    if (lastMod != m_lastFrameModified) {
        m_lastFrameModified = lastMod;
        m_frameRefreshCounter++;
        // 使用 query 参数让 QML Image 认为是新 URL，绕过缓存
        m_frameImageUrl = QString("file://%1?t=%2")
                              .arg(FRAME_PATH)
                              .arg(m_frameRefreshCounter);
        emit frameUpdated();
    }
}
