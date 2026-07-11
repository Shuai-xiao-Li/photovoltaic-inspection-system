import QtQuick 2.12
import QtQuick.Controls 2.5

Item {
    id: root

    // 毛玻璃底
    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.04)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ── 标题与模式切换 ──
        Row {
            width: parent.width
            Text {
                text: "🕹️ 手动控制区"
                color: Qt.rgba(1, 1, 1, 0.9)
                font.pixelSize: 14
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }
            
            Item { width: 1; height: 1; Component.onCompleted: width = Qt.binding(function() { return parent.width - 150 }) }

            Switch {
                id: modeSwitch
                text: checked ? "手动模式" : "自动巡检"
                checked: typeof vehicleData !== "undefined" ? vehicleData.currentMode === 1 : false
                onCheckedChanged: {
                    if (typeof vehicleData !== "undefined") {
                        vehicleData.currentMode = checked ? 1 : 0
                    }
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1) }

        // ── 十字方向键与急停 ──
        Item {
            width: parent.width
            height: 120
            opacity: modeSwitch.checked ? 1.0 : 0.3 // 自动模式下置灰不可用
            enabled: modeSwitch.checked

            // 上
            Rectangle {
                width: 44; height: 44; radius: 8
                color: upMa.pressed ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                border.color: Qt.rgba(1, 1, 1, 0.15)
                anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top
                Text { anchors.centerIn: parent; text: "▲"; font.pixelSize: 18; color: "white" }
                MouseArea { id: upMa; anchors.fill: parent; onClicked: if(typeof vehicleData !== "undefined") vehicleData.sendControlCommand(1, 0) }
            }
            // 下
            Rectangle {
                width: 44; height: 44; radius: 8
                color: downMa.pressed ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                border.color: Qt.rgba(1, 1, 1, 0.15)
                anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
                Text { anchors.centerIn: parent; text: "▼"; font.pixelSize: 18; color: "white" }
                MouseArea { id: downMa; anchors.fill: parent; onClicked: if(typeof vehicleData !== "undefined") vehicleData.sendControlCommand(2, 0) }
            }
            // 左
            Rectangle {
                width: 44; height: 44; radius: 8
                color: leftMa.pressed ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                border.color: Qt.rgba(1, 1, 1, 0.15)
                anchors.verticalCenter: parent.verticalCenter; anchors.right: stopBtn.left; anchors.rightMargin: 8
                Text { anchors.centerIn: parent; text: "◀"; font.pixelSize: 18; color: "white" }
                MouseArea { id: leftMa; anchors.fill: parent; onClicked: if(typeof vehicleData !== "undefined") vehicleData.sendControlCommand(3, 0) }
            }
            // 右
            Rectangle {
                width: 44; height: 44; radius: 8
                color: rightMa.pressed ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                border.color: Qt.rgba(1, 1, 1, 0.15)
                anchors.verticalCenter: parent.verticalCenter; anchors.left: stopBtn.right; anchors.leftMargin: 8
                Text { anchors.centerIn: parent; text: "▶"; font.pixelSize: 18; color: "white" }
                MouseArea { id: rightMa; anchors.fill: parent; onClicked: if(typeof vehicleData !== "undefined") vehicleData.sendControlCommand(4, 0) }
            }

            // 居中急停按钮
            Rectangle {
                id: stopBtn
                width: 50; height: 50; radius: 25
                anchors.centerIn: parent
                color: stopMa.pressed ? "#b91c1c" : "#ef4444" // 醒目红色
                border.color: "#fca5a5"
                border.width: 2
                Text { anchors.centerIn: parent; text: "STOP"; font.pixelSize: 12; font.weight: Font.Bold; color: "white" }
                
                // 红色发光
                Rectangle {
                    anchors.fill: parent; radius: 25; color: "transparent"; border.color: "#ef4444"; border.width: 4
                    opacity: 0.4
                }

                MouseArea {
                    id: stopMa
                    anchors.fill: parent
                    onClicked: if(typeof vehicleData !== "undefined") vehicleData.sendControlCommand(5, 0)
                }
            }
        }

        // ── 速度控制滑块 ──
        Row {
            width: parent.width
            spacing: 12
            opacity: modeSwitch.checked ? 1.0 : 0.3
            enabled: modeSwitch.checked

            Text {
                text: "速度"
                color: Qt.rgba(1, 1, 1, 0.7)
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Slider {
                id: speedSlider
                from: 0; to: 100; value: 50
                width: parent.width - 60
                anchors.verticalCenter: parent.verticalCenter
                onValueChanged: {
                    if(typeof vehicleData !== "undefined" && pressed) {
                        vehicleData.sendControlCommand(6, Math.round(value))
                    }
                }
            }
        }
    }
}
