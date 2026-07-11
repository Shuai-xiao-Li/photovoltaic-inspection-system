/**
 * @file panelmodel.h
 * @brief 光伏板状态数据模型类头文件，定义 QAbstractListModel 数据角色及接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef PANELMODEL_H
#define PANELMODEL_H

#include <QAbstractListModel>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>

struct PanelInfo {
    QString id;
    QString status;       // "NORMAL", "SHADED", "DAMAGED"
    double confidence;
    QString lastCheckTime;
    QString description;
    QVariantList events;
    double voltage;
    double current;
    double power;
    QString commStatus;   // "GOOD", "FAIR", "POOR"
};

class PanelModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        StatusRole,
        ConfidenceRole,
        LastCheckTimeRole,
        DescriptionRole
    };

    explicit PanelModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap getPanelDetail(int index) const;

    // ── 接收真实检测数据并更新面板列表 ──
    void handleDetectionUpdate(const QString &className, double confidence, double inferMs);

signals:
    void countChanged();

private slots:
    void simulateStatusChange();

private:
    QList<PanelInfo> m_panels;
    QTimer m_simTimer;
    void initSimulatedData();
    QString currentTimeStr() const;

    QString m_lastDetectedClass; // 保存上一次检测到的类别
    int m_panelCounter = 5;      // 面板计数器，初始为 5 (对应初始 PV-001 到 PV-005)
};

#endif // PANELMODEL_H
