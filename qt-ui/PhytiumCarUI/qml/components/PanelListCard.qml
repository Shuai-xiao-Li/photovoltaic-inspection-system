import QtQuick 2.12

Item {
    id: root
    width: 360
    height: 480

    // ── Signals ─────────────────────────────────────────────
    signal panelClicked(int index)

    // ── Status helpers ──────────────────────────────────────
    function statusColor(status) {
        if (status === "NORMAL")  return "#34d399";
        if (status === "SHADED")  return "#fbbf24";
        if (status === "DAMAGED") return "#ef4444";
        return Qt.rgba(1, 1, 1, 0.30);
    }

    function statusLabel(status) {
        if (status === "NORMAL")  return "正常";
        if (status === "SHADED")  return "遮挡";
        if (status === "DAMAGED") return "损坏";
        return status;
    }

    GlassCard {
        anchors.fill: parent

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // ── Title row ───────────────────────────────────
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: "光伏板检测"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: Qt.rgba(1, 1, 1, 0.92)
                    anchors.verticalCenter: parent.verticalCenter
                }

                // Count badge
                Rectangle {
                    width: badgeText.implicitWidth + 14
                    height: 22
                    radius: 11
                    color: Qt.rgba(0.309804, 0.556863, 0.968627, 0.18)
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: typeof panelModel !== "undefined" ? panelModel.count : "0"
                        font.pixelSize: 12
                        font.weight: Font.Bold
                        color: "#4f8ef7"
                    }
                }
            }

            // ── Separator ───────────────────────────────────
            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.08)
            }

            // ── Panel list ──────────────────────────────────
            ListView {
                id: listView
                width: parent.width
                height: parent.height - 50
                clip: true
                spacing: 2
                model: typeof panelModel !== "undefined" ? panelModel : null

                // ── Smooth add / remove ─────────────────────
                add: Transition {
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: 300; easing.type: Easing.OutCubic }
                    NumberAnimation { properties: "y"; from: 20; duration: 300; easing.type: Easing.OutCubic }
                }
                remove: Transition {
                    NumberAnimation { properties: "opacity"; to: 0; duration: 250; easing.type: Easing.InCubic }
                }
                displaced: Transition {
                    NumberAnimation { properties: "y"; duration: 300; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: delegateRoot
                    width: listView.width
                    height: 52
                    radius: 10
                    color: delegateMA.pressed  ? Qt.rgba(1, 1, 1, 0.06)
                         : delegateMA.containsMouse ? Qt.rgba(1, 1, 1, 0.03)
                         : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        // Status dot
                        Rectangle {
                            width: 10; height: 10
                            radius: 5
                            color: statusColor(model.status)
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // Panel ID
                        Text {
                            text: model.panelId
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: Qt.rgba(1, 1, 1, 0.92)
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width * 0.25
                            elide: Text.ElideRight
                        }

                        // Status text
                        Text {
                            text: statusLabel(model.status)
                            font.pixelSize: 13
                            color: statusColor(model.status)
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width * 0.2
                        }

                        // Time
                        Text {
                            text: model.lastCheckTime || ""
                            font.pixelSize: 12
                            color: Qt.rgba(1, 1, 1, 0.35)
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            width: parent.width * 0.3
                        }
                    }

                    MouseArea {
                        id: delegateMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.panelClicked(model.index)
                    }
                }
            }
        }
    }
}
