/**
 * @file detectionbridge.h
 * @brief Python AI 算法与 C++ 界面层通信桥梁类头文件，声明 IPC 属性与轮询接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef DETECTIONBRIDGE_H
#define DETECTIONBRIDGE_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QDateTime>

/**
 * DetectionBridge — Python 检测算法 ↔ Qt UI 数据桥梁
 *
 * 通过轮询 /tmp/phytium_detection.json 获取检测结果，
 * 通过轮询 /tmp/phytium_frame.jpg 获取带标注的实时帧。
 * 所有数据以 Q_PROPERTY 形式暴露给 QML 层绑定。
 */
class DetectionBridge : public QObject
{
    Q_OBJECT

    // ── 检测结果属性 ──
    Q_PROPERTY(QString className READ className NOTIFY detectionUpdated)
    Q_PROPERTY(double confidence READ confidence NOTIFY detectionUpdated)
    Q_PROPERTY(int centerX READ centerX NOTIFY detectionUpdated)
    Q_PROPERTY(int centerY READ centerY NOTIFY detectionUpdated)
    Q_PROPERTY(int flag READ flag NOTIFY detectionUpdated)
    Q_PROPERTY(double inferTimeMs READ inferTimeMs NOTIFY detectionUpdated)
    Q_PROPERTY(bool cameraOk READ cameraOk NOTIFY detectionUpdated)
    Q_PROPERTY(int detectionCount READ detectionCount NOTIFY detectionUpdated)
    Q_PROPERTY(int occlusionCount READ occlusionCount NOTIFY detectionUpdated)
    Q_PROPERTY(int damageCount READ damageCount NOTIFY detectionUpdated)
    Q_PROPERTY(double detectionFps READ detectionFps NOTIFY detectionUpdated)
    Q_PROPERTY(int uptimeSeconds READ uptimeSeconds NOTIFY detectionUpdated)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY detectionUpdated)

    // ── 实时帧图片 URL（QML Image source 绑定用）──
    Q_PROPERTY(QString frameImageUrl READ frameImageUrl NOTIFY frameUpdated)

    // ── 算法在线状态 ──
    Q_PROPERTY(bool algorithmOnline READ algorithmOnline NOTIFY detectionUpdated)

public:
    explicit DetectionBridge(QObject *parent = nullptr);

    // ── Getters ──
    QString className() const { return m_className; }
    double confidence() const { return m_confidence; }
    int centerX() const { return m_centerX; }
    int centerY() const { return m_centerY; }
    int flag() const { return m_flag; }
    double inferTimeMs() const { return m_inferTimeMs; }
    bool cameraOk() const { return m_cameraOk; }
    int detectionCount() const { return m_detectionCount; }
    int occlusionCount() const { return m_occlusionCount; }
    int damageCount() const { return m_damageCount; }
    double detectionFps() const { return m_detectionFps; }
    int uptimeSeconds() const { return m_uptimeSeconds; }
    int frameCount() const { return m_frameCount; }
    QString frameImageUrl() const { return m_frameImageUrl; }
    bool algorithmOnline() const { return m_algorithmOnline; }

signals:
    void detectionUpdated();
    void frameUpdated();

private slots:
    void pollDetectionData();
    void pollFrameImage();

private:
    QTimer m_jsonTimer;
    QTimer m_frameTimer;

    // ── 检测数据 ──
    QString m_className;
    double m_confidence = 0.0;
    int m_centerX = 0;
    int m_centerY = 0;
    int m_flag = 0;
    double m_inferTimeMs = 0.0;
    bool m_cameraOk = false;
    int m_detectionCount = 0;
    int m_occlusionCount = 0;
    int m_damageCount = 0;
    double m_detectionFps = 0.0;
    int m_uptimeSeconds = 0;
    int m_frameCount = 0;

    // ── 帧 URL ──
    QString m_frameImageUrl;
    qint64 m_lastFrameModified = 0;
    int m_frameRefreshCounter = 0;

    // ── 算法在线判断 ──
    bool m_algorithmOnline = false;
    qint64 m_lastJsonTimestamp = 0;

    // ── IPC 文件路径 ──
    static const QString JSON_PATH;
    static const QString FRAME_PATH;
};

#endif // DETECTIONBRIDGE_H
