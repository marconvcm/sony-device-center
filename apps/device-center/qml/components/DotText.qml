import QtQuick
import ".."
import "../DotGlyphs.js" as Glyphs

// Dot-matrix display text. Every glyph sits on a 5x7 grid of round dots and
// new text sweeps in column by column. Anything the grid can't show (kana,
// symbols) falls back to the bold body face so nothing ever renders blank.
Item {
    id: root
    property string text: ""
    property real dot: 4
    property real pitch: dot * 1.55
    property color color: Theme.txt
    // Shrinks the grid when the text would overflow, so long translations
    // still land on one line.
    property real maxWidth: 0
    property bool fallback: false
    readonly property int rows: 7
    readonly property real naturalWidth: Math.max(0, columns * pitch - (pitch - dot))
    readonly property real fit: maxWidth > 0 && naturalWidth > maxWidth ? maxWidth / naturalWidth : 1
    readonly property real p: pitch * fit
    readonly property real d: dot * fit

    property var dots: []
    property int columns: 0
    property real progress: 1
    // Milliseconds before the sweep starts, so neighbouring displays that
    // update together don't flip in unison.
    property int delay: 0

    implicitWidth: fallback ? fallbackText.implicitWidth : naturalWidth * fit
    implicitHeight: fallback ? fallbackText.implicitHeight : (rows * pitch - (pitch - dot)) * fit
    Accessible.role: Accessible.StaticText
    Accessible.name: text

    function rebuild() {
        var chars = Array.from(Glyphs.normalize(text))
        var points = [], x = 0, missing = false
        for (var i = 0; i < chars.length; ++i) {
            var glyph = Glyphs.table[chars[i]]
            if (glyph === undefined) { missing = true; break }
            var width = glyph[0].length
            for (var r = 0; r < rows; ++r)
                for (var c = 0; c < width; ++c)
                    // The third number is a stable per-dot jitter: some dots
                    // land early, some late, but the sweep still runs left to right.
                    if (glyph[r].charAt(c) === "#") points.push(x + c, r, ((x + c) * 37 + r * 91 + i * 17) % 100 / 100)
            x += width + 1
        }
        dots = points
        columns = Math.max(0, x - 1)
        fallback = missing
        if (Theme.motionEnabled && visible) { progress = 0; sweep.restart() }
        else { sweep.stop(); progress = 1 }
        canvas.requestPaint()
    }

    onTextChanged: rebuild()
    onColorChanged: canvas.requestPaint()
    onPChanged: canvas.requestPaint()
    onDChanged: canvas.requestPaint()
    onProgressChanged: canvas.requestPaint()
    Component.onCompleted: rebuild()

    SequentialAnimation {
        id: sweep
        PauseAnimation { duration: root.delay }
        NumberAnimation {
            target: root; property: "progress"
            from: 0; to: 1
            duration: 520 + root.columns * 16
            easing.type: Easing.OutQuad
        }
    }
    Connections {
        target: Theme
        function onMotionEnabledChanged() { if (!Theme.motionEnabled) { sweep.stop(); root.progress = 1 } }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        visible: !root.fallback
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var wave = 3, jitter = 6
            var reach = root.progress * (root.columns + wave + jitter)
            var r = root.d / 2
            for (var i = 0; i < root.dots.length; i += 3) {
                var col = root.dots[i]
                var alpha = Math.max(0, Math.min(1, (reach - col - root.dots[i + 2] * jitter) / wave))
                if (alpha <= 0) continue
                ctx.fillStyle = Qt.rgba(root.color.r, root.color.g, root.color.b, root.color.a * alpha)
                ctx.beginPath()
                ctx.arc(col * root.p + r, root.dots[i + 1] * root.p + r, r, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }

    Text {
        id: fallbackText
        visible: root.fallback
        text: root.text
        color: root.color
        textFormat: Text.PlainText
        font.pixelSize: root.pitch * root.rows * 0.92
        width: root.maxWidth > 0 ? root.maxWidth : implicitWidth
        elide: Text.ElideRight
        font.weight: Font.Bold
        font.letterSpacing: -0.5
    }
}
