import QtQuick 2.12

Item {
    id: root
    width: 200
    height: 56

    // ── Public API ──────────────────────────────────────────
    property int    percent: typeof vehicleData !== "undefined" ? vehicleData.batteryPercent : 0
    property double voltage: typeof vehicleData !== "undefined" ? vehicleData.batteryVoltage : 0

    // ── Derived ─────────────────────────────────────────────
    readonly property color fillColor: percent > 50 ? "#34d399"
                                     : percent > 20 ? "#fbbf24"
                                                     : "#ef4444"

    Row {
        anchors.fill: parent
        spacing: root.width * 0.06

        // ── Battery icon ────────────────────────────────────
        Item {
            id: batteryIcon
            width: root.width * 0.42
            height: root.height * 0.55
            anchors.verticalCenter: parent.verticalCenter

            // Body outline
            Rectangle {
                id: batteryBody
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - tipRect.width - 2
                height: parent.height
                radius: height * 0.2
                color: "transparent"
                border.color: Qt.rgba(1, 1, 1, 0.25)
                border.width: 1.5

                // Fill
                Rectangle {
                    id: fillRect
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height - 6
                    width: (parent.width - 6) * Math.max(0, Math.min(root.percent, 100)) / 100
                    radius: parent.radius * 0.6
                    color: root.fillColor

                    Behavior on width {
                        NumberAnimation { duration: 600; easing.type: Easing.OutCubic }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 400; easing.type: Easing.OutCubic }
                    }
                }
            }

            // Tip
            Rectangle {
                id: tipRect
                anchors.left: batteryBody.right
                anchors.leftMargin: 1
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.07
                height: parent.height * 0.35
                radius: width * 0.4
                color: Qt.rgba(1, 1, 1, 0.25)
            }
        }

        // ── Text column ─────────────────────────────────────
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: root.percent + "%"
                font.pixelSize: root.height * 0.32
                font.weight: Font.Bold
                color: root.fillColor

                Behavior on color {
                    ColorAnimation { duration: 400; easing.type: Easing.OutCubic }
                }
            }

            Text {
                text: root.voltage.toFixed(1) + " V"
                font.pixelSize: root.height * 0.2
                color: Qt.rgba(1, 1, 1, 0.45)
            }
        }
    }
}
