import QtQuick
import ".."

// A dot-matrix number you can click to type. Enter applies, Escape or
// clicking away cancels; the value is clamped to [from, to].
Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property bool signed: false
    property real dot: 4
    property color color: Theme.txt
    signal edited(int v)

    readonly property bool editing: field.visible
    // The width is reserved for the widest value in range, sign included.
    // Otherwise "0" (no sign) is narrower than "+8", the slider next to it
    // grows by a glyph mid-drag and the handle lands on another value.
    implicitWidth: Math.max(widest.implicitWidth, field.visible ? field.implicitWidth : 0)
    implicitHeight: display.implicitHeight

    function open() {
        field.text = String(value)
        field.visible = true
        field.forceActiveFocus()
        field.selectAll()
    }
    // Type a value programmatically: what Enter does after typing.
    function apply(text) { open(); field.text = String(text); commit() }
    function commit() {
        var v = parseInt(field.text, 10)
        field.visible = false
        if (isNaN(v)) return
        v = Math.max(from, Math.min(to, v))
        if (v !== value) root.edited(v)
    }

    DotText {
        id: widest
        visible: false
        dot: root.dot
        text: (root.signed || root.from < 0 ? "-" : "")
            + "8".repeat(Math.max(String(Math.abs(root.from)).length, String(Math.abs(root.to)).length))
    }
    DotText {
        id: display
        anchors.right: parent.right
        text: (root.signed && root.value > 0 ? "+" : "") + root.value
        dot: root.dot
        color: root.color
        visible: !field.visible
        opacity: root.enabled ? 1 : 0.5
    }
    HoverHandler { enabled: root.enabled && !field.visible; cursorShape: Qt.IBeamCursor }
    TapHandler { enabled: root.enabled && !field.visible; onTapped: root.open() }

    Rectangle {
        visible: field.visible
        anchors.fill: field
        anchors.margins: -6
        radius: Theme.controlRadius
        color: Theme.surfaceHi
        border.width: 1
        border.color: Theme.accent
    }
    TextInput {
        id: field
        visible: false
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(44, implicitWidth)
        horizontalAlignment: TextInput.AlignRight
        color: Theme.txt
        font.pixelSize: Math.max(14, root.dot * 4)
        font.weight: Font.Bold
        selectByMouse: true
        selectionColor: Theme.accent
        selectedTextColor: Theme.accentText
        validator: IntValidator { bottom: Math.min(root.from, -99); top: Math.max(root.to, 999) }
        onAccepted: root.commit()
        onActiveFocusChanged: if (!activeFocus && visible) visible = false
        Keys.onEscapePressed: visible = false
    }
}
