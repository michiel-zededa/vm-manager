import QtQuick
import VMManager

// Rolling sparkline. Feed it a `value` (it keeps a ring buffer) and it animates
// a filled line chart. Used for the per-VM CPU/mem trend on cards.
Item {
    id: spark
    property real value: 0            // latest sample
    property real maxValue: 100       // full-scale
    property int samples: 40
    property color lineColor: Theme.accent
    property color fillColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)

    property var _buf: []

    onValueChanged: {
        _buf.push(Math.max(0, Math.min(value, maxValue)));
        while (_buf.length > samples)
            _buf.shift();
        canvas.requestPaint();
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const n = spark._buf.length;
            if (n < 2) return;
            const w = width, h = height;
            const dx = w / (spark.samples - 1);
            const x0 = w - (n - 1) * dx;
            const y = v => h - (v / spark.maxValue) * (h - 2) - 1;

            ctx.beginPath();
            ctx.moveTo(x0, y(spark._buf[0]));
            for (let i = 1; i < n; ++i)
                ctx.lineTo(x0 + i * dx, y(spark._buf[i]));

            // Fill under the line.
            ctx.lineTo(x0 + (n - 1) * dx, h);
            ctx.lineTo(x0, h);
            ctx.closePath();
            ctx.fillStyle = spark.fillColor;
            ctx.fill();

            // Stroke.
            ctx.beginPath();
            ctx.moveTo(x0, y(spark._buf[0]));
            for (let i = 1; i < n; ++i)
                ctx.lineTo(x0 + i * dx, y(spark._buf[i]));
            ctx.lineWidth = 1.5;
            ctx.strokeStyle = spark.lineColor;
            ctx.stroke();
        }
    }
}
