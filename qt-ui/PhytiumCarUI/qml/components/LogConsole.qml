import QtQuick 2.12

Item {
    id: root

    // 毛玻璃底
    Rectangle {
        anchors.fill: parent
        radius: 8
        color: Qt.rgba(1, 1, 1, 0.04)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    ListView {
        id: logList
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        spacing: 4

        // 绑定到底层的 systemLogs (QStringList)
        model: typeof vehicleData !== "undefined" ? vehicleData.systemLogs : []

        delegate: Text {
            text: modelData
            width: logList.width
            wrapMode: Text.Wrap
            font.pixelSize: Math.max(10, root.height * 0.15)
            font.family: "Courier New" // 等宽字体更适合日志
            
            // 根据内容简单变色
            color: text.indexOf("告警") !== -1 || text.indexOf("异常") !== -1 ? "#ef4444" 
                 : text.indexOf("指令") !== -1 ? "#f59e0b"
                 : Qt.rgba(1, 1, 1, 0.7)
        }

        // 当新日志插入时，自动滚动到底部
        onCountChanged: {
            logList.positionViewAtEnd()
        }
    }
}
