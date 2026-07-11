import QtQuick 2.12

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
        spacing: 16

        // ── 标题 ──
        Text {
            text: "🗺️ 巡检任务与路径进度"
            color: Qt.rgba(1, 1, 1, 0.9)
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1) }

        // ── 任务信息 ──
        Text {
            text: "当前任务: " + (typeof vehicleData !== "undefined" ? vehicleData.currentTaskName : "未知任务")
            color: Qt.rgba(1, 1, 1, 0.8)
            font.pixelSize: 13
        }

        // ── 进度条 ──
        Item {
            width: parent.width
            height: 30
            
            property int inspected: typeof vehicleData !== "undefined" ? vehicleData.inspectedPanels : 0
            property int total: typeof vehicleData !== "undefined" ? vehicleData.totalPanels : 1
            property real progress: total > 0 ? inspected / total : 0
            
            // 进度条底底色
            Rectangle {
                width: parent.width
                height: 8
                radius: 4
                color: Qt.rgba(1, 1, 1, 0.1)
                anchors.verticalCenter: parent.verticalCenter
                
                // 进度条发光
                Rectangle {
                    width: parent.width * parent.progress
                    height: 8
                    radius: 4
                    color: "#4f8ef7"
                    
                    // 动画平滑进度
                    Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
                }
            }
            
            // 进度文字
            Text {
                text: parent.inspected + " / " + parent.total + " 块"
                color: Qt.rgba(1, 1, 1, 0.6)
                font.pixelSize: 11
                anchors.right: parent.right
                anchors.top: parent.bottom
                anchors.topMargin: 2
            }
        }

        // ── 目标与预计时间 ──
        Grid {
            columns: 2
            spacing: 20
            width: parent.width

            Column {
                spacing: 4
                Text { text: "下一目标点"; color: Qt.rgba(1, 1, 1, 0.5); font.pixelSize: 11 }
                Text { 
                    text: typeof vehicleData !== "undefined" ? vehicleData.nextTarget : "--"
                    color: "#34d399"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            Column {
                spacing: 4
                Text { text: "预计剩余时间 (ETA)"; color: Qt.rgba(1, 1, 1, 0.5); font.pixelSize: 11 }
                
                property int eta: typeof vehicleData !== "undefined" ? vehicleData.etaSeconds : 0
                Text { 
                    text: Math.floor(parent.eta / 60) + " 分 " + (parent.eta % 60) + " 秒"
                    color: "#fbbf24"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
