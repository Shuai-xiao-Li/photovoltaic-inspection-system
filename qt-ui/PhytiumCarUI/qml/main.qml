import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12
import "components"

ApplicationWindow {
    id: root
    visible: true
    visibility: Window.FullScreen
    title: "飞腾巡检小车"
    color: "#0a0a0f"

    // ── 自适应缩放因子 ──
    readonly property real sf: Math.max(0.65, Math.min(height / 1080.0, width / 1920.0))
    readonly property real marginSize: width * 0.012

    // ═══════════════════════════════════════════════════
    //  背景层
    // ═══════════════════════════════════════════════════
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0d0d1a" }
            GradientStop { position: 0.4; color: "#0f1628" }
            GradientStop { position: 1.0; color: "#080810" }
        }
    }

    // 光晕
    Rectangle {
        width: parent.width * 0.5; height: parent.height * 0.5
        x: parent.width * 0.3; y: -parent.height * 0.1
        radius: width / 2
        opacity: 0.06
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#4f8ef7" }
            GradientStop { position: 1.0; color: "#a855f7" }
        }
    }

    // ═══════════════════════════════════════════════════
    //  主内容区
    // ═══════════════════════════════════════════════════
    Item {
        anchors.fill: parent
        anchors.margins: marginSize

        // 1. 顶部状态栏
        StatusBar {
            id: statusBar
            width: parent.width
            height: Math.max(44, root.height * 0.05)
            anchors.top: parent.top
        }

        // 2. 底部日志台
        LogConsole {
            id: logConsole
            width: parent.width
            height: root.height * 0.12
            anchors.bottom: parent.bottom
        }

        // 3. 中间核心工作区
        Item {
            anchors.top: statusBar.bottom
            anchors.bottom: logConsole.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: marginSize
            anchors.bottomMargin: marginSize

            // 左侧：视觉主中心 (算法实时帧) + 底部悬浮的折叠面板
            Item {
                id: cameraArea
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: height * (4.0 / 3.0)

                // ── 算法实时帧画面（来自 Python 端输出的带标注 JPEG）──
                Image {
                    id: cameraView
                    anchors.fill: parent
                    anchors.margins: 16
                    source: typeof detectionBridge !== "undefined" ? detectionBridge.frameImageUrl : ""
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    asynchronous: false
                    visible: source !== ""
                }

                // ── 算法离线占位画面 ──
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 16
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.03)
                    visible: !cameraView.visible

                    Column {
                        anchors.centerIn: parent
                        spacing: 16

                        Text {
                            text: "📹"
                            font.pixelSize: 48
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: "等待算法启动..."
                            color: Qt.rgba(1, 1, 1, 0.4)
                            font.pixelSize: 16
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: "请先运行 Python 检测算法"
                            color: Qt.rgba(1, 1, 1, 0.25)
                            font.pixelSize: 12
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }

                // 视频框外侧遮幅（模拟圆角）
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.color: "#0a0a0f"
                    border.width: 16
                    radius: 24
                }

                // 视频框内侧高光装饰边框
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 15
                    color: "transparent"
                    border.color: Qt.rgba(1, 1, 1, 0.15)
                    border.width: 1
                    radius: 9
                }

                // 悬浮在左下角的"检测结果面板" (折叠/展开)
                DetectionPanel {
                    id: detectionPanel
                    width: parent.width * 0.6
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: marginSize
                }
            }

            // 右侧：系统状态 + 巡检进度
            Column {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * 0.33
                spacing: marginSize

                SystemStatusCard {
                    width: parent.width
                    height: parent.height * 0.55
                }

                TaskProgressCard {
                    width: parent.width
                    height: parent.height * 0.45 - marginSize
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════
    //  右侧隐藏抽屉：光伏板详细列表
    // ═══════════════════════════════════════════════════
    Drawer {
        id: panelDrawer
        width: root.width * 0.28
        height: root.height
        edge: Qt.RightEdge
        background: Rectangle { color: "transparent" } // 让卡片本身发光

        PanelListCard {
            anchors.fill: parent
            anchors.margins: marginSize
            
            // 点击项时打开详情弹窗
            onPanelClicked: {
                detailModal.panelIndex = index
                detailModal.visible = true
            }
        }
    }

    // 悬浮在右侧边缘的"呼出抽屉"按钮
    Rectangle {
        width: 30; height: 100; radius: 10
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        color: Qt.rgba(1, 1, 1, 0.1)
        border.color: Qt.rgba(1, 1, 1, 0.2)
        visible: !panelDrawer.opened

        Text {
            anchors.centerIn: parent
            text: "◀"
            color: "white"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: panelDrawer.open()
        }
    }

    // ═══════════════════════════════════════════════════
    //  光伏板详情弹窗
    // ═══════════════════════════════════════════════════
    PanelDetailModal {
        id: detailModal
        anchors.fill: parent
        visible: false
        onClosed: detailModal.visible = false
    }

    // ═══════════════════════════════════════════════════
    //  全局初始入场动画
    // ═══════════════════════════════════════════════════
    NumberAnimation {
        id: rootFadeIn
        target: root.contentItem
        property: "opacity"
        from: 0; to: 1; duration: 800
        easing.type: Easing.OutCubic
    }

    Component.onCompleted: rootFadeIn.start()
}
