pragma Singleton
import QtQuick 2.12

QtObject {
    // ── 背景层 ──
    readonly property color bgPrimary:   "#0a0a0f"
    readonly property color bgSecondary: "#1c1c1e"
    readonly property color bgElevated:  "#2c2c2e"

    // ── 玻璃态 ──
    readonly property color glassBg:     Qt.rgba(1, 1, 1, 0.06)
    readonly property color glassBorder: Qt.rgba(1, 1, 1, 0.12)

    // ── 强调色 ──
    readonly property color accentBlue:   "#4f8ef7"
    readonly property color accentPurple: "#a855f7"
    readonly property color accentGreen:  "#34d399"
    readonly property color accentYellow: "#fbbf24"
    readonly property color accentRed:    "#ef4444"

    // ── 文字 ──
    readonly property color textPrimary:   Qt.rgba(1, 1, 1, 0.92)
    readonly property color textSecondary: Qt.rgba(1, 1, 1, 0.55)
    readonly property color textTertiary:  Qt.rgba(1, 1, 1, 0.30)

    // ── 圆角 ──
    readonly property int radiusLarge:  20
    readonly property int radiusMedium: 12
    readonly property int radiusSmall:  8

    // ── 动画 ──
    readonly property int animDuration: 300
}
