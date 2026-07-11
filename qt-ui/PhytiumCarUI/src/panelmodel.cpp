/**
 * @file panelmodel.cpp
 * @brief 光伏板状态数据模型类实现，维护近期检测到的光伏板信息列表，将新异常节点动态呈现在侧边栏中。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "panelmodel.h"
#include <QDateTime>
#include <QRandomGenerator>

PanelModel::PanelModel(QObject *parent)
    : QAbstractListModel(parent)
{
    initSimulatedData();
    // 禁用模拟定时器，不再随机改变状态，改为接收真实算法数据
    // connect(&m_simTimer, &QTimer::timeout, this, &PanelModel::simulateStatusChange);
    // m_simTimer.start(8000); 
}

void PanelModel::initSimulatedData()
{
    QStringList statuses  = {"NORMAL", "SHADED", "DAMAGED", "NORMAL", "SHADED"};
    QStringList descs     = {
        "工作正常，各项指标良好",
        "检测到部分遮挡",
        "发现表面裂纹",
        "工作正常，功率稳定",
        "轻微灰尘覆盖"
    };

    for (int i = 0; i < 5; ++i) {
        PanelInfo panel;
        panel.id             = QString("PV-%1").arg(i + 1, 3, 10, QChar('0'));
        panel.status         = statuses[i];
        panel.confidence     = 0.85 + QRandomGenerator::global()->generateDouble() * 0.14;
        panel.lastCheckTime  = QDateTime::currentDateTime().addSecs(-i * 120).toString("HH:mm:ss");
        panel.description    = descs[i];
        panel.voltage        = 36.0 + QRandomGenerator::global()->generateDouble() * 2.0;
        panel.current        = 7.5 + QRandomGenerator::global()->generateDouble() * 1.5;
        panel.power          = panel.voltage * panel.current;
        panel.commStatus     = (i < 3) ? "GOOD" : "FAIR";

        // 初始事件
        QVariantMap ev1;
        ev1["time"]   = panel.lastCheckTime;
        ev1["status"] = panel.status;
        ev1["desc"]   = panel.description;
        panel.events.append(ev1);

        QVariantMap ev2;
        ev2["time"]   = QDateTime::currentDateTime().addSecs(-i * 120 - 300).toString("HH:mm:ss");
        ev2["status"] = "NORMAL";
        ev2["desc"]   = "首次检测";
        panel.events.append(ev2);

        m_panels.append(panel);
    }
}

int PanelModel::rowCount(const QModelIndex &) const
{
    return m_panels.count();
}

QVariant PanelModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_panels.count())
        return QVariant();

    const PanelInfo &p = m_panels[index.row()];
    switch (role) {
    case IdRole:            return p.id;
    case StatusRole:        return p.status;
    case ConfidenceRole:    return p.confidence;
    case LastCheckTimeRole: return p.lastCheckTime;
    case DescriptionRole:   return p.description;
    default:                return QVariant();
    }
}

QHash<int, QByteArray> PanelModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole]            = "panelId";
    roles[StatusRole]        = "status";
    roles[ConfidenceRole]    = "confidence";
    roles[LastCheckTimeRole] = "lastCheckTime";
    roles[DescriptionRole]   = "description";
    return roles;
}

QVariantMap PanelModel::getPanelDetail(int index) const
{
    if (index < 0 || index >= m_panels.count())
        return QVariantMap();

    const PanelInfo &p = m_panels[index];
    QVariantMap detail;
    detail["id"]            = p.id;
    detail["status"]        = p.status;
    detail["confidence"]    = p.confidence;
    detail["lastCheckTime"] = p.lastCheckTime;
    detail["description"]   = p.description;
    detail["events"]        = p.events;
    detail["voltage"]       = p.voltage;
    detail["current"]       = p.current;
    detail["power"]         = p.power;
    detail["commStatus"]    = p.commStatus;
    return detail;
}

void PanelModel::simulateStatusChange()
{
    int idx = QRandomGenerator::global()->bounded(m_panels.count());
    QStringList sts = {"NORMAL", "SHADED", "DAMAGED"};
    QStringList dsc = {"恢复正常", "检测到遮挡", "发现损伤"};
    int si = QRandomGenerator::global()->bounded(3);

    m_panels[idx].status        = sts[si];
    m_panels[idx].lastCheckTime = currentTimeStr();
    m_panels[idx].confidence    = 0.85 + QRandomGenerator::global()->generateDouble() * 0.14;
    m_panels[idx].description   = dsc[si];

    QVariantMap ev;
    ev["time"]   = m_panels[idx].lastCheckTime;
    ev["status"] = sts[si];
    ev["desc"]   = dsc[si];
    m_panels[idx].events.prepend(ev);

    QModelIndex mi = createIndex(idx, 0);
    emit dataChanged(mi, mi);
}

QString PanelModel::currentTimeStr() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void PanelModel::handleDetectionUpdate(const QString &className, double confidence, double inferMs)
{
    Q_UNUSED(inferMs);
    
    // 如果类别为空，代表没有检测到面板，不记录
    if (className.isEmpty()) {
        m_lastDetectedClass = "";
        return;
    }

    // 转换算法类别到 UI 状态
    // Clean -> NORMAL
    // Dusty, Bird-drop, Snow-Covered -> SHADED
    // Physical-Damage, Electrical-damage -> DAMAGED
    QString status = "NORMAL";
    QString desc = "正常工作";
    if (className == "Clean") {
        status = "NORMAL";
        desc = "正常工作";
    } else if (className == "Dusty") {
        status = "SHADED";
        desc = "检测到灰尘覆盖";
    } else if (className == "Bird-drop") {
        status = "SHADED";
        desc = "检测到鸟粪遮挡";
    } else if (className == "Snow-Covered") {
        status = "SHADED";
        desc = "检测到积雪覆盖";
    } else if (className == "Physical-Damage") {
        status = "DAMAGED";
        desc = "光伏板物理破损";
    } else if (className == "Electrical-damage") {
        status = "DAMAGED";
        desc = "光伏板电气故障";
    } else {
        status = "SHADED";
        desc = className;
    }

    QString timeStr = currentTimeStr();

    // 判断是新面板还是同一个面板的更新
    // 如果上一次的类别是空的，或者类别发生了变化，我们判定为一个新面板的识别事件
    if (m_lastDetectedClass.isEmpty() || m_lastDetectedClass != className) {
        m_lastDetectedClass = className;
        m_panelCounter++;

        // 插入新面板到列表最上方 (index = 0)
        beginInsertRows(QModelIndex(), 0, 0);
        
        PanelInfo panel;
        panel.id             = QString("PV-%1").arg(m_panelCounter, 3, 10, QChar('0'));
        panel.status         = status;
        panel.confidence     = confidence;
        panel.lastCheckTime  = timeStr;
        panel.description    = desc;
        panel.voltage        = 36.0 + QRandomGenerator::global()->generateDouble() * 2.0;
        panel.current        = 7.5 + QRandomGenerator::global()->generateDouble() * 1.5;
        panel.power          = panel.voltage * panel.current;
        panel.commStatus     = "GOOD";

        // 添加事件记录
        QVariantMap ev1;
        ev1["time"]   = timeStr;
        ev1["status"] = status;
        ev1["desc"]   = desc;
        panel.events.append(ev1);

        m_panels.prepend(panel);
        endInsertRows();
        
        emit countChanged();

        // 限制列表最大长度为 20，防止无限增长占用内存
        if (m_panels.count() > 20) {
            beginRemoveRows(QModelIndex(), m_panels.count() - 1, m_panels.count() - 1);
            m_panels.removeLast();
            endRemoveRows();
            emit countChanged();
        }
    } else {
        // 同一个面板的连续更新：更新当前最上方的面板数据
        if (!m_panels.isEmpty()) {
            m_panels[0].status        = status;
            m_panels[0].confidence    = confidence;
            m_panels[0].lastCheckTime = timeStr;
            m_panels[0].description   = desc;

            // 如果状态变了，添加新事件
            if (m_panels[0].events.isEmpty() || m_panels[0].events[0].toMap()["status"].toString() != status) {
                QVariantMap ev;
                ev["time"]   = timeStr;
                ev["status"] = status;
                ev["desc"]   = desc;
                m_panels[0].events.prepend(ev);
            }

            QModelIndex mi = index(0, 0);
            emit dataChanged(mi, mi);
        }
    }
}
