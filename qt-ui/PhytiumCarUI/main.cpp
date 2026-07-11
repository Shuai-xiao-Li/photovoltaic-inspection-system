/**
 * @file main.cpp
 * @brief QGuiApplication 初始化入口，负责加载 QML 引擎、向 QML 上下文注册全局 C++ 核心数据模型与通信对象，并连接 AI 算法输出与底盘指标的信号槽关系。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFont>
#include "src/vehicledata.h"
#include "src/panelmodel.h"
#include "src/detectionbridge.h"

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    // 设置默认字体
    QFont defaultFont("Inter");
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(defaultFont);

    QQmlApplicationEngine engine;

    // 注册 C++ 数据对象到 QML
    VehicleData vehicleData;
    PanelModel panelModel;
    DetectionBridge detectionBridge;
    engine.rootContext()->setContextProperty("vehicleData", &vehicleData);
    engine.rootContext()->setContextProperty("panelModel", &panelModel);
    engine.rootContext()->setContextProperty("detectionBridge", &detectionBridge);

    // 连接信号：当后台算法检测更新时，同步更新侧边栏面板列表，同时更新已巡检面板数
    QObject::connect(&detectionBridge, &DetectionBridge::detectionUpdated,
                     &panelModel, [&detectionBridge, &panelModel, &vehicleData]() {
        panelModel.handleDetectionUpdate(
            detectionBridge.className(),
            detectionBridge.confidence(),
            detectionBridge.inferTimeMs()
        );
        vehicleData.updateInspectionStats(detectionBridge.detectionCount(), 120);
    });

    // 添加 QML 模块搜索路径
    engine.addImportPath("qrc:/qml");

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

