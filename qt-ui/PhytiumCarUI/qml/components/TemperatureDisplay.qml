import QtQuick 2.12

Item {
    id: root
    width: 160
    height: 64

    // ── Public API ──────────────────────────────────────────
    property double temperature: typeof vehicleData !== "undefined" ? vehicleData.temperature : 0

    // ── Color coding ────────────────────────────────────────
    readonly property color valueColor: temperature > 38 ? "#ef4444"
                                      : temperature >= 30 ? "#fbbf24"
                                                          : "#34d399"

    Column {
        anchors.fill: parent
        spacing: root.height * 0.06

        // ── Section label ───────────────────────────────────
        Text {
            text: "车内温度"
            font.pixelSize: root.height * 0.17
            color: Qt.rgba(1, 1, 1, 0.55)
        }

        // ── Value row ───────────────────────────────────────
        Row {
            spacing: root.width * 0.04

            Text {
                text: "🌡"
                font.pixelSize: root.height * 0.3
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: root.temperature.toFixed(1) + "°C"
                font.pixelSize: root.height * 0.32
                font.weight: Font.DemiBold
                color: root.valueColor
                anchors.verticalCenter: parent.verticalCenter

                Behavior on color {
                    ColorAnimation { duration: 400; easing.type: Easing.OutCubic }
                }
            }
        }
    }
}
