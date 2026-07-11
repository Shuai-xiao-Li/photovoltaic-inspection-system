import QtQuick 2.12

Item {
    id: root

    // ── 算法真实数据 ──
    property bool algoOnline: typeof detectionBridge !== "undefined" ? detectionBridge.algorithmOnline : false
    property string currentClass: typeof detectionBridge !== "undefined" ? detectionBridge.className : ""
    property double realFps: typeof detectionBridge !== "undefined" ? detectionBridge.detectionFps : 0

    // ── 毛玻璃底 ──
    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.04)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: Math.max(12, parent.width * 0.02)

        // ── 连接状态 ──
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Rectangle {
                width: 8; height: 8; radius: 4
                color: vehicleData.serialConnected ? "#34d399" : "#ef4444"
                anchors.verticalCenter: parent.verticalCenter

                SequentialAnimation on opacity {
                    running: vehicleData.serialConnected
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.4; duration: 1200 }
                    NumberAnimation { to: 1.0; duration: 1200 }
                }
            }
            Text {
                text: vehicleData.serialConnected ? "串口 已连接" : "串口 未连接"
                color: Qt.rgba(1, 1, 1, 0.7)
                font.pixelSize: Math.max(11, root.height * 0.28)
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // ── 分隔符 ──
        Rectangle {
            width: 1; height: parent.height * 0.4
            color: Qt.rgba(1, 1, 1, 0.1)
            anchors.verticalCenter: parent.verticalCenter
        }

        // ── AI 检测状态（真实数据）──
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Rectangle {
                width: 8; height: 8; radius: 4
                color: root.algoOnline ? "#34d399" : "#ef4444"
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: {
                    if (!root.algoOnline) return "AI 离线"
                    if (root.currentClass === "") return "AI 待机"
                    if (root.currentClass === "Clean") return "AI ✓ 正常"
                    return "AI ⚠ " + root.currentClass
                }
                color: {
                    if (!root.algoOnline) return "#ef4444"
                    if (root.currentClass === "Clean") return "#34d399"
                    if (root.currentClass === "") return Qt.rgba(1, 1, 1, 0.7)
                    return "#fbbf24"
                }
                font.pixelSize: Math.max(11, root.height * 0.28)
                font.weight: root.algoOnline ? Font.DemiBold : Font.Normal
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            width: 1; height: parent.height * 0.4
            color: Qt.rgba(1, 1, 1, 0.1)
            anchors.verticalCenter: parent.verticalCenter
        }

        // ── FPS（算法真实帧率）──
        Text {
            text: root.algoOnline ? "FPS " + root.realFps.toFixed(1) : "FPS --"
            color: Qt.rgba(1, 1, 1, 0.7)
            font.pixelSize: Math.max(11, root.height * 0.28)
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1; height: parent.height * 0.4
            color: Qt.rgba(1, 1, 1, 0.1)
            anchors.verticalCenter: parent.verticalCenter
        }

        // ── 故障状态 ──
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Rectangle {
                width: 8; height: 8; radius: 4
                color: vehicleData.faultCode === 0 ? "#34d399" : "#ef4444"
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: vehicleData.faultCode === 0 ? "系统正常" : "故障 0x" + vehicleData.faultCode.toString(16).toUpperCase()
                color: vehicleData.faultCode === 0 ? Qt.rgba(1, 1, 1, 0.7) : "#ef4444"
                font.pixelSize: Math.max(11, root.height * 0.28)
                anchors.verticalCenter: parent.verticalCenter
            }
        }

    }

    // ── 运行时间 ──
    Text {
        text: "⏱ " + vehicleData.uptimeStr
        color: Qt.rgba(1, 1, 1, 0.5)
        font.pixelSize: Math.max(11, root.height * 0.28)
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
    }
}
