import QtQuick 2.12

GlassCard {
    id: root

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // ── 标题 ──
        Text {
            text: "🖥️ 核心系统状态"
            color: Qt.rgba(1, 1, 1, 0.9)
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1) }

        // ── 硬件在线状态 ──
        Grid {
            columns: 2
            spacing: 12
            width: parent.width

            // 飞腾派
            Row {
                spacing: 6
                Rectangle { width: 8; height: 8; radius: 4; color: "#34d399"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "飞腾派: 在线"; color: Qt.rgba(1, 1, 1, 0.8); font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
            }
            // STM32
            Row {
                id: stmRow
                spacing: 6
                property bool stmOk: typeof vehicleData !== "undefined" ? vehicleData.serialConnected : false
                Rectangle { width: 8; height: 8; radius: 4; color: stmRow.stmOk ? "#34d399" : "#ef4444"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: stmRow.stmOk ? "STM32: 已连接" : "STM32: 断开"; color: Qt.rgba(1, 1, 1, 0.8); font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
            }
            // AI 算法状态（真实数据）
            Row {
                id: algoRow
                spacing: 6
                property bool algoOk: typeof detectionBridge !== "undefined" ? detectionBridge.algorithmOnline : false
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: algoRow.algoOk ? "#34d399" : "#ef4444"
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        running: algoRow.algoOk
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.4; duration: 1500 }
                        NumberAnimation { to: 1.0; duration: 1500 }
                    }
                }
                Text {
                    text: algoRow.algoOk ? "AI 算法: 运行中" : "AI 算法: 离线"
                    color: Qt.rgba(1, 1, 1, 0.8)
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            // 摄像头状态（真实数据）
            Row {
                id: camRow
                spacing: 6
                property bool camOk: typeof detectionBridge !== "undefined" ? detectionBridge.cameraOk : false
                Rectangle { width: 8; height: 8; radius: 4; color: camRow.camOk ? "#34d399" : "#ef4444"; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: camRow.camOk ? "摄像头: 正常" : "摄像头: 未就绪"
                    color: Qt.rgba(1, 1, 1, 0.8)
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            // 电机状态
            Row {
                id: motorRow
                spacing: 6
                property int motor: typeof vehicleData !== "undefined" ? vehicleData.motorStatus : 0
                Rectangle { width: 8; height: 8; radius: 4; color: motorRow.motor === 0 ? "#34d399" : "#ef4444"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: motorRow.motor === 0 ? "电机驱动: 正常" : "电机驱动: 异常"; color: Qt.rgba(1, 1, 1, 0.8); font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1) }

        // ── 仪表数据 ──
        SpeedGauge {
            width: parent.width
            height: root.height * 0.35 // 动态高度
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1) }

        Row {
            width: parent.width
            height: Math.max(48, root.height * 0.12)
            spacing: 10

            BatteryIndicator {
                width: parent.width * 0.5 - 6
                height: parent.height
            }

            Rectangle {
                width: 1
                height: parent.height * 0.6
                color: Qt.rgba(1, 1, 1, 0.1)
                anchors.verticalCenter: parent.verticalCenter
            }

            TemperatureDisplay {
                width: parent.width * 0.5 - 6
                height: parent.height
            }
        }
    }
}
