import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 200
    height: 200

    property real progress: 0.0 // 0.0 to 1.0
    property string text: Math.round(progress * 100) + "%"
    property color glowColor: "#FF416C"
    property color trackColor: Qt.rgba(255/255, 65/255, 108/255, 0.2) // Subtle red/pink for the track
    property color progressColor: glowColor
    property real lineWidth: 10

    onProgressChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var centerX = width / 2;
            var centerY = height / 2;
            var radius = Math.min(centerX, centerY) - root.lineWidth;

            // Draw track (Inner loop)
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, 0, Math.PI * 2, false);
            ctx.lineWidth = root.lineWidth;
            ctx.strokeStyle = root.trackColor;
            ctx.stroke();

            // Draw outer loop (slightly smaller/larger based on design, mock shows 2 rings)
            // Let's do a faint outer ring
            var outerRadius = radius + root.lineWidth + 2;
            ctx.beginPath();
            ctx.arc(centerX, centerY, outerRadius, 0, Math.PI * 2, false);
            ctx.lineWidth = 2;
            ctx.strokeStyle = root.trackColor;
            ctx.stroke();

            // Draw progress
            if (root.progress > 0) {
                ctx.beginPath();
                // start at top (-PI/2)
                var startAngle = -Math.PI / 2;
                var endAngle = startAngle + (Math.PI * 2 * root.progress);
                ctx.arc(centerX, centerY, radius, startAngle, endAngle, false);
                ctx.lineWidth = root.lineWidth;
                ctx.strokeStyle = root.progressColor;
                ctx.lineCap = "round";

                // Add glow
                ctx.shadowColor = root.progressColor;
                ctx.shadowBlur = 15;
                ctx.stroke();

                // Draw outer progress ring
                ctx.beginPath();
                ctx.arc(centerX, centerY, outerRadius, startAngle, endAngle, false);
                ctx.lineWidth = 2;
                ctx.strokeStyle = root.progressColor;
                ctx.lineCap = "round";
                ctx.shadowBlur = 5;
                ctx.stroke();
            }
        }
    }

    Label {
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: root.height / 4
        font.bold: true
        color: "#FFFFFF"
    }
}
