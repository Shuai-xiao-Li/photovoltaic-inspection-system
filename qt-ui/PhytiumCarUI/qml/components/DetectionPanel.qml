import QtQuick 2.12

Item {
    id: root
    
    property bool isExpanded: false
    
    // 折叠和展开的高度
    readonly property real collapsedHeight: 60
    readonly property real expandedHeight: 280
    
    // 动画绑定自身高度
    height: isExpanded ? expandedHeight : collapsedHeight
    Behavior on height {
        NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
    }

    // ── 算法在线状态 ──
    property bool algoOnline: typeof detectionBridge !== "undefined" ? detectionBridge.algorithmOnline : false
    property string currentClass: typeof detectionBridge !== "undefined" ? detectionBridge.className : ""
    property double currentConf: typeof detectionBridge !== "undefined" ? detectionBridge.confidence : 0
    property int occCount: typeof detectionBridge !== "undefined" ? detectionBridge.occlusionCount : 0
    property int dmgCount: typeof detectionBridge !== "undefined" ? detectionBridge.damageCount : 0
    property double inferMs: typeof detectionBridge !== "undefined" ? detectionBridge.inferTimeMs : 0

    // ── 毛玻璃卡片背景 ──
    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.05)
        border.color: isExpanded ? "#4f8ef7" : Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
        
        Behavior on border.color { ColorAnimation { duration: 300 } }
    }

    // ── 顶部摘要行 (始终可见) ──
    Item {
        id: summaryHeader
        width: parent.width
        height: root.collapsedHeight
        
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14
            
            // 算法在线指示灯
            Rectangle {
                width: 8; height: 8; radius: 4
                color: root.algoOnline ? "#34d399" : "#ef4444"
                anchors.verticalCenter: parent.verticalCenter

                SequentialAnimation on opacity {
                    running: root.algoOnline
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.4; duration: 1200 }
                    NumberAnimation { to: 1.0; duration: 1200 }
                }
            }
            
            Text {
                text: root.algoOnline ? "🔍 AI 检测中" : "🔍 AI 离线"
                color: Qt.rgba(1, 1, 1, 0.9)
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            
            // 当前分类实时显示
            Text {
                visible: root.algoOnline && root.currentClass !== ""
                text: root.currentClass === "Clean" ? "✅ 正常" :
                      root.currentClass === "Dusty" ? "⚠️ 灰尘" :
                      root.currentClass === "Bird-drop" ? "⚠️ 鸟粪" :
                      root.currentClass === "Snow-Covered" ? "⚠️ 积雪" :
                      root.currentClass === "Physical-Damage" ? "🔴 物理损伤" :
                      root.currentClass === "Electrical-damage" ? "🔴 电气损伤" : root.currentClass
                color: root.currentClass === "Clean" ? "#34d399" :
                       (root.currentClass === "Physical-Damage" || root.currentClass === "Electrical-damage") ? "#ef4444" : "#fbbf24"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            
            Text {
                visible: root.algoOnline && root.currentConf > 0
                text: (root.currentConf * 100).toFixed(0) + "%"
                color: Qt.rgba(1, 1, 1, 0.5)
                font.pixelSize: 12
            }

            // 分隔竖线
            Rectangle {
                width: 1; height: 20
                color: Qt.rgba(1, 1, 1, 0.1)
                anchors.verticalCenter: parent.verticalCenter
            }
            
            Text {
                text: "遮挡: " + root.occCount
                color: "#fbbf24"
                font.pixelSize: 13
            }
            
            Text {
                text: "破损: " + root.dmgCount
                color: "#ef4444"
                font.pixelSize: 13
            }
        }
        
        // 展开/收起按钮文本
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.isExpanded ? "收起 ▲" : "展开详情 ▼"
            color: "#4f8ef7"
            font.pixelSize: 12
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.isExpanded = !root.isExpanded
        }
    }

    // ── 展开后的详细内容区 ──
    Item {
        id: detailContent
        anchors.top: summaryHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        opacity: root.isExpanded ? 1.0 : 0.0
        visible: opacity > 0
        clip: true
        
        Behavior on opacity {
            NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.1)
        }

        Column {
            anchors.fill: parent
            anchors.topMargin: 10
            spacing: 10

            // ── 实时检测数据面板 ──
            Row {
                spacing: 20
                width: parent.width

                // 推理耗时
                Column {
                    spacing: 2
                    Text { text: "推理耗时"; color: Qt.rgba(1, 1, 1, 0.4); font.pixelSize: 11 }
                    Text {
                        text: root.algoOnline ? root.inferMs.toFixed(1) + " ms" : "--"
                        color: root.inferMs > 200 ? "#ef4444" : root.inferMs > 100 ? "#fbbf24" : "#34d399"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }

                // 检测次数
                Column {
                    spacing: 2
                    Text { text: "检测次数"; color: Qt.rgba(1, 1, 1, 0.4); font.pixelSize: 11 }
                    Text {
                        text: typeof detectionBridge !== "undefined" ? detectionBridge.detectionCount : 0
                        color: "#4f8ef7"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }

                // 中心坐标
                Column {
                    spacing: 2
                    Text { text: "目标坐标"; color: Qt.rgba(1, 1, 1, 0.4); font.pixelSize: 11 }
                    Text {
                        text: root.algoOnline ? "X:" + (typeof detectionBridge !== "undefined" ? detectionBridge.centerX : 0)
                                                + " Y:" + (typeof detectionBridge !== "undefined" ? detectionBridge.centerY : 0) : "--"
                        color: Qt.rgba(1, 1, 1, 0.8)
                        font.pixelSize: 13
                    }
                }
            }

            Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.06) }

            // ── 分类统计列表 ──
            Text {
                text: "异常检测记录"
                color: Qt.rgba(1, 1, 1, 0.5)
                font.pixelSize: 11
            }

            ListView {
                width: parent.width
                height: parent.height - 100
                clip: true
                spacing: 6
                model: root.occCount + root.dmgCount
                
                delegate: Rectangle {
                    width: parent.width
                    height: 36
                    radius: 6
                    color: Qt.rgba(1, 1, 1, 0.03)
                    border.color: Qt.rgba(1, 1, 1, 0.05)
                    
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        spacing: 10
                        
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: index < root.dmgCount ? "#ef4444" : "#fbbf24"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: index < root.dmgCount ? "光伏板损伤" : "遮挡/灰尘"
                            color: Qt.rgba(1, 1, 1, 0.8)
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}
