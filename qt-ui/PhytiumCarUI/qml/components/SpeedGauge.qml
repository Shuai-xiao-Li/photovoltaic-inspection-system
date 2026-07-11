import QtQuick 2.12

Item {
    id: root
    width: 220
    height: 220

    // ── Public API ──────────────────────────────────────────
    property double value: typeof vehicleData !== "undefined" ? vehicleData.speed : 0
    property double minValue: 0
    property double maxValue: 80

    // ── Internal animated value ─────────────────────────────
    property double displayValue: 0
    Behavior on displayValue {
        NumberAnimation { duration: 800; easing.type: Easing.OutCubic }
    }
    onValueChanged: displayValue = Math.max(minValue, Math.min(value, maxValue))
    Component.onCompleted: displayValue = Math.max(minValue, Math.min(value, maxValue))
    onDisplayValueChanged: canvas.requestPaint()

    // ── Derived geometry ────────────────────────────────────
    readonly property real dim: Math.min(width, height)
    readonly property real arcWidth: dim * 0.07
    readonly property real arcRadius: (dim - arcWidth) * 0.5

    // ── Arc angles (degrees → radians) ──────────────────────
    readonly property real startAngle: 135 * Math.PI / 180
    readonly property real endAngle:   405 * Math.PI / 180
    readonly property real sweepRange: endAngle - startAngle          // 270°

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();

            var cx = width  * 0.5;
            var cy = height * 0.5;
            var r  = root.arcRadius;
            var lw = root.arcWidth;

            // ── background arc ──────────────────────────────
            ctx.beginPath();
            ctx.arc(cx, cy, r, root.startAngle, root.endAngle, false);
            ctx.strokeStyle = "rgba(255,255,255,0.08)";
            ctx.lineWidth   = lw;
            ctx.lineCap     = "round";
            ctx.stroke();

            // ── value arc (gradient blue → purple) ──────────
            var fraction = (root.displayValue - root.minValue)
                         / (root.maxValue - root.minValue);
            if (fraction > 0.005) {
                var valEnd = root.startAngle + root.sweepRange * fraction;

                // Gradient runs along the arc direction
                var gx0 = cx + r * Math.cos(root.startAngle);
                var gy0 = cy + r * Math.sin(root.startAngle);
                var gx1 = cx + r * Math.cos(valEnd);
                var gy1 = cy + r * Math.sin(valEnd);

                var grad = ctx.createLinearGradient(gx0, gy0, gx1, gy1);
                grad.addColorStop(0.0, "#4f8ef7");
                grad.addColorStop(1.0, "#a855f7");

                ctx.beginPath();
                ctx.arc(cx, cy, r, root.startAngle, valEnd, false);
                ctx.strokeStyle = grad;
                ctx.lineWidth   = lw;
                ctx.lineCap     = "round";
                ctx.stroke();
            }

            // ── tick marks (every 10 km/h) ──────────────────
            ctx.strokeStyle = "rgba(255,255,255,0.15)";
            ctx.lineWidth   = 1;
            ctx.lineCap     = "butt";
            for (var t = 0; t <= root.maxValue; t += 10) {
                var tickFrac  = t / root.maxValue;
                var tickAngle = root.startAngle + root.sweepRange * tickFrac;
                var inner     = r - lw * 0.7;
                var outer     = r + lw * 0.7;
                ctx.beginPath();
                ctx.moveTo(cx + inner * Math.cos(tickAngle),
                           cy + inner * Math.sin(tickAngle));
                ctx.lineTo(cx + outer * Math.cos(tickAngle),
                           cy + outer * Math.sin(tickAngle));
                ctx.stroke();
            }
        }
    }

    // ── Speed number ────────────────────────────────────────
    Text {
        id: speedText
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -root.dim * 0.03
        text: Math.round(root.displayValue).toString()
        font.pixelSize: root.dim * 0.22
        font.weight: Font.Light
        font.family: "Helvetica Neue"
        color: "#FFFFFF"
        opacity: 0.92
        horizontalAlignment: Text.AlignHCenter
    }

    // ── Unit label ──────────────────────────────────────────
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: speedText.bottom
        anchors.topMargin: root.dim * 0.01
        text: "km/h"
        font.pixelSize: root.dim * 0.07
        color: "#FFFFFF"
        opacity: 0.45
        horizontalAlignment: Text.AlignHCenter
    }
}
