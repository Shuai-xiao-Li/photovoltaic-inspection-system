import QtQuick 2.12
import QtGraphicalEffects 1.12

Item {
    id: root
    default property alias content: contentArea.data
    property real cardRadius: 20
    property color cardColor: Qt.rgba(1, 1, 1, 0.06)
    property color borderColor: Qt.rgba(1, 1, 1, 0.12)
    property bool enableShadow: true

    // ── 卡片背景 + 阴影 ──
    Rectangle {
        id: cardBg
        anchors.fill: parent
        radius: cardRadius
        color: cardColor
        border.color: borderColor
        border.width: 1

        // 顶部高光线
        Rectangle {
            anchors.top: parent.top
            anchors.topMargin: 1
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.6
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.3; color: Qt.rgba(1, 1, 1, 0.12) }
                GradientStop { position: 0.7; color: Qt.rgba(1, 1, 1, 0.12) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        layer.enabled: enableShadow
        layer.effect: DropShadow {
            transparentBorder: true
            horizontalOffset: 0
            verticalOffset: 8
            radius: 24
            samples: 25
            color: Qt.rgba(0, 0, 0, 0.35)
            cached: true
        }
    }

    // ── 内容区域 ──
    Item {
        id: contentArea
        anchors.fill: parent
        anchors.margins: parent.width > 300 ? 24 : 16
    }
}
