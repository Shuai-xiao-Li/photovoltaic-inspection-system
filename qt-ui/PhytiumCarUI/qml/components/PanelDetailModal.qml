import QtQuick 2.12

Item {
    id: root
    anchors.fill: parent

    // ── Public API ──────────────────────────────────────────
    property bool   modalVisible: false
    property int    panelIndex: -1
    signal closed()

    // ── Detail data (loaded on open) ────────────────────────
    property var detail: ({})

    onModalVisibleChanged: {
        if (modalVisible && panelIndex >= 0 && typeof panelModel !== "undefined") {
            detail = panelModel.getPanelDetail(panelIndex) || {};
        }
        state = modalVisible ? "open" : "closed";
    }

    // ── Convenience accessors ───────────────────────────────
    readonly property string detailId:          detail.id          || ""
    readonly property string detailStatus:      detail.status      || ""
    readonly property real   detailConfidence:  detail.confidence  || 0
    readonly property string detailLastCheck:   detail.lastCheckTime || ""
    readonly property string detailDescription: detail.description || ""
    readonly property var    detailEvents:      detail.events      || []
    readonly property real   detailVoltage:     detail.voltage     || 0
    readonly property real   detailCurrent:     detail.current     || 0
    readonly property real   detailPower:       detail.power       || 0
    readonly property string detailCommStatus:  detail.commStatus  || ""

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

    // ── States & transitions ────────────────────────────────
    visible: false
    state: "closed"

    states: [
        State {
            name: "open"
            PropertyChanges { target: root;     visible: true }
            PropertyChanges { target: overlay;  opacity: 1 }
            PropertyChanges { target: cardContainer; opacity: 1; scale: 1.0 }
        },
        State {
            name: "closed"
            PropertyChanges { target: overlay;  opacity: 0 }
            PropertyChanges { target: cardContainer; opacity: 0; scale: 0.95 }
        }
    ]

    transitions: [
        Transition {
            from: "closed"; to: "open"
            SequentialAnimation {
                PropertyAction { target: root; property: "visible"; value: true }
                ParallelAnimation {
                    NumberAnimation { target: overlay; property: "opacity"; duration: 300; easing.type: Easing.OutCubic }
                    NumberAnimation { target: cardContainer; properties: "opacity,scale"; duration: 300; easing.type: Easing.OutCubic }
                }
            }
        },
        Transition {
            from: "open"; to: "closed"
            SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation { target: overlay; property: "opacity"; duration: 250; easing.type: Easing.InCubic }
                    NumberAnimation { target: cardContainer; properties: "opacity,scale"; duration: 250; easing.type: Easing.InCubic }
                }
                PropertyAction { target: root; property: "visible"; value: false }
            }
        }
    ]

    // ── Dark overlay ────────────────────────────────────────
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)
        opacity: 0

        MouseArea {
            anchors.fill: parent
            onClicked: {
                root.modalVisible = false;
                root.closed();
            }
        }
    }

    // ── Center card container ───────────────────────────────
    Item {
        id: cardContainer
        anchors.centerIn: parent
        width: parent.width * 0.7
        height: parent.height * 0.8
        opacity: 0
        scale: 0.95
        transformOrigin: Item.Center

        // Prevent clicks from reaching overlay
        MouseArea {
            anchors.fill: parent
            onClicked: { /* absorb */ }
        }

        GlassCard {
            anchors.fill: parent

            Flickable {
                id: contentFlick
                anchors.fill: parent
                anchors.margins: 20
                contentHeight: contentColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: contentColumn
                    width: parent.width
                    spacing: 14

                    // ── Top row: back + title ────────────────
                    Row {
                        width: parent.width
                        spacing: 12

                        Text {
                            text: "← 返回"
                            font.pixelSize: 15
                            color: "#4f8ef7"
                            anchors.verticalCenter: parent.verticalCenter

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.modalVisible = false;
                                    root.closed();
                                }
                            }
                        }

                        Text {
                            text: root.detailId
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            color: Qt.rgba(1, 1, 1, 0.92)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // ── Status badge ─────────────────────────
                    Row {
                        spacing: 12

                        Rectangle {
                            width: statusBadgeText.implicitWidth + 20
                            height: 28
                            radius: 14
                            color: Qt.rgba(
                                statusColor(root.detailStatus).r,
                                statusColor(root.detailStatus).g,
                                statusColor(root.detailStatus).b,
                                0.15
                            )

                            Text {
                                id: statusBadgeText
                                anchors.centerIn: parent
                                text: statusLabel(root.detailStatus)
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: statusColor(root.detailStatus)
                            }
                        }

                        Text {
                            text: "置信度: " + Math.round(root.detailConfidence) + "%"
                            font.pixelSize: 14
                            color: Qt.rgba(1, 1, 1, 0.55)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // ── Description ──────────────────────────
                    Text {
                        width: parent.width
                        text: root.detailDescription
                        font.pixelSize: 13
                        color: Qt.rgba(1, 1, 1, 0.55)
                        wrapMode: Text.WordWrap
                        visible: text !== ""
                    }

                    // ── Last check time ──────────────────────
                    Text {
                        text: "上次检测: " + root.detailLastCheck
                        font.pixelSize: 12
                        color: Qt.rgba(1, 1, 1, 0.35)
                        visible: root.detailLastCheck !== ""
                    }

                    // ── Separator ────────────────────────────
                    Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

                    // ── Events section ───────────────────────
                    Text {
                        text: "检测事件"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Qt.rgba(1, 1, 1, 0.92)
                    }

                    Column {
                        width: parent.width
                        spacing: 6

                        Repeater {
                            model: root.detailEvents

                            Rectangle {
                                width: parent.width
                                height: eventCol.height + 16
                                radius: 10
                                color: Qt.rgba(1, 1, 1, 0.03)

                                Column {
                                    id: eventCol
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    Row {
                                        spacing: 8

                                        Rectangle {
                                            width: 8; height: 8; radius: 4
                                            color: statusColor(modelData.status || "")
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: modelData.time || ""
                                            font.pixelSize: 12
                                            color: Qt.rgba(1, 1, 1, 0.45)
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: statusLabel(modelData.status || "")
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            color: statusColor(modelData.status || "")
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        text: modelData.description || ""
                                        font.pixelSize: 12
                                        color: Qt.rgba(1, 1, 1, 0.55)
                                        wrapMode: Text.WordWrap
                                        visible: text !== ""
                                    }
                                }
                            }
                        }

                        // Empty state
                        Text {
                            visible: root.detailEvents.length === 0
                            text: "暂无检测事件"
                            font.pixelSize: 13
                            color: Qt.rgba(1, 1, 1, 0.30)
                        }
                    }

                    // ── Separator ────────────────────────────
                    Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

                    // ── Communication status ─────────────────
                    Text {
                        text: "通信状况"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Qt.rgba(1, 1, 1, 0.92)
                    }

                    Row {
                        spacing: 8

                        Rectangle {
                            width: 10; height: 10; radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: {
                                var cs = root.detailCommStatus;
                                if (cs === "ONLINE" || cs === "NORMAL" || cs === "OK")
                                    return "#34d399";
                                if (cs === "UNSTABLE" || cs === "WEAK")
                                    return "#fbbf24";
                                return "#ef4444";
                            }
                        }

                        Text {
                            text: root.detailCommStatus || "N/A"
                            font.pixelSize: 14
                            color: Qt.rgba(1, 1, 1, 0.75)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // ── Separator ────────────────────────────
                    Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

                    // ── Sensor data ──────────────────────────
                    Text {
                        text: "传感器数据"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Qt.rgba(1, 1, 1, 0.92)
                    }

                    Row {
                        width: parent.width
                        spacing: 0

                        // Voltage column
                        Column {
                            width: parent.width / 3
                            spacing: 4

                            Text {
                                text: "电压"
                                font.pixelSize: 12
                                color: Qt.rgba(1, 1, 1, 0.45)
                            }
                            Text {
                                text: root.detailVoltage.toFixed(2) + " V"
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: "#4f8ef7"
                            }
                        }

                        // Current column
                        Column {
                            width: parent.width / 3
                            spacing: 4

                            Text {
                                text: "电流"
                                font.pixelSize: 12
                                color: Qt.rgba(1, 1, 1, 0.45)
                            }
                            Text {
                                text: root.detailCurrent.toFixed(2) + " A"
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: "#a855f7"
                            }
                        }

                        // Power column
                        Column {
                            width: parent.width / 3
                            spacing: 4

                            Text {
                                text: "功率"
                                font.pixelSize: 12
                                color: Qt.rgba(1, 1, 1, 0.45)
                            }
                            Text {
                                text: root.detailPower.toFixed(2) + " W"
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: "#fbbf24"
                            }
                        }
                    }

                    // Bottom spacer
                    Item { width: 1; height: 10 }
                }
            }
        }
    }
}
