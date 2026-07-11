import QtQuick 2.12

Item {
    id: root
    width: 220
    height: 72

    // ── Public API ──────────────────────────────────────────
    property double latitude:  typeof vehicleData !== "undefined" ? vehicleData.latitude  : 0
    property double longitude: typeof vehicleData !== "undefined" ? vehicleData.longitude : 0

    // ── Formatting helpers ──────────────────────────────────
    readonly property string latText: {
        var prefix = latitude >= 0 ? "N " : "S ";
        return prefix + Math.abs(latitude).toFixed(4) + "°";
    }
    readonly property string lonText: {
        var prefix = longitude >= 0 ? "E " : "W ";
        return prefix + Math.abs(longitude).toFixed(4) + "°";
    }

    Column {
        anchors.fill: parent
        spacing: root.height * 0.06

        // ── Section label ───────────────────────────────────
        Text {
            text: "当前位置"
            font.pixelSize: root.height * 0.17
            color: Qt.rgba(1, 1, 1, 0.55)
        }

        // ── Coordinates row ─────────────────────────────────
        Row {
            spacing: root.width * 0.04

            Text {
                text: "📍"
                font.pixelSize: root.height * 0.26
                anchors.verticalCenter: parent.verticalCenter
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    text: root.latText
                    font.pixelSize: root.height * 0.2
                    font.family: "Menlo"
                    color: Qt.rgba(1, 1, 1, 0.92)
                }

                Text {
                    text: root.lonText
                    font.pixelSize: root.height * 0.2
                    font.family: "Menlo"
                    color: Qt.rgba(1, 1, 1, 0.92)
                }
            }
        }
    }
}
