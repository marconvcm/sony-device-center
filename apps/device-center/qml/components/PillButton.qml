import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts

// The button. Active is solid ink; idle is an outlined sheet. Press physics
// live here so every button feels identical.
Button {
    required property var appWindow
    id: pill
    property color tint: Theme.accent
    property bool active: false
    property string glyphPath: ""
    property bool compact: false
    property bool trailingChevron: false

    implicitHeight: compact ? 38 : 50
    topPadding: 0
    bottomPadding: 0
    leftPadding: compact ? 14 : 20
    rightPadding: leftPadding
    implicitWidth: pillRow.implicitWidth + leftPadding + rightPadding
    hoverEnabled: true
    scale: pressed ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.duration(140); easing.type: Easing.OutQuad } }

    readonly property color ink: active ? Theme.accentText : Theme.txt

    background: Rectangle {
        radius: Theme.controlRadius
        color: pill.active ? pill.tint : pill.hovered ? Theme.surfaceHi : Theme.surface
        border.width: 1
        border.color: pill.active ? pill.tint : pill.hovered ? Theme.lineHi : Theme.line
        Behavior on color { ColorAnimation { duration: Theme.tBase } }
        Behavior on border.color { ColorAnimation { duration: Theme.tBase } }
    }

    contentItem: RowLayout {
        id: pillRow
        spacing: pill.compact ? 8 : 10
        Glyph { appWindow: pill.appWindow;
            visible: pill.glyphPath !== ""
            path: pill.glyphPath
            size: pill.compact ? 16 : 20
            weight: 1.7
            color: pill.ink
        }
        Text {
            textFormat: Text.PlainText
            // A button squeezed below its content (fillWidth in a narrow
            // card, "Открыть эквалайзер" at the minimum window) elides
            // rather than running out over its own edge.
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: pill.text
            color: pill.ink
            font.pixelSize: pill.compact ? 12 : 13
            font.weight: Font.Medium
            Behavior on color { ColorAnimation { duration: Theme.tFast } }
        }
        Glyph { appWindow: pill.appWindow;
            visible: pill.trailingChevron
            path: appWindow.icons.chevronRight
            size: 14
            weight: 1.8
            color: pill.ink
        }
    }
}
