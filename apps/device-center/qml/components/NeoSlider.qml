import QtQuick
import ".."
import QtQuick.Controls

// Horizontal slider: a thin sunk track, ink fill, paper knob.
Slider {
    required property var appWindow
    id: sl
    property real confirmedValue: 0
    value: confirmedValue
    // The knob shows what the user asked for until the device has gone quiet:
    // never under the finger, never while a command is in flight, and not for
    // a moment after release, because the snapshot from an intermediate value
    // used to snap a just-released knob back to where it passed through.
    function sync() { if (!pressed && !settle.running && !controller.busy) value = confirmedValue }
    onConfirmedValueChanged: sync()
    onMoved: settle.restart()
    onPressedChanged: {
        settle.restart()
        if (!pressed && Math.round(value) !== Math.round(confirmedValue)) moved()
    }
    Connections { target: controller; function onStateChanged() { if (!controller.busy) sl.sync() } }
    Timer { id: settle; interval: 600; onTriggered: sl.sync() }
    implicitHeight: 28
    hoverEnabled: true

    background: Rectangle {
        x: sl.leftPadding
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: sl.availableWidth
        height: 6
        radius: 3
        color: Theme.surfaceSunk
        Rectangle {
            width: sl.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: 22
        height: 22
        radius: 11
        color: Theme.surfaceHi
        border.width: 1
        border.color: Theme.lineHi
        scale: sl.pressed ? 1.12 : (sl.hovered ? 1.06 : 1.0)
        Behavior on scale { NumberAnimation { duration: Theme.duration(140) } }
        Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: Theme.accent }
    }
}
