import QtQuick
import ".."
import QtQuick.Controls

// Toggle. Ink track with a paper knob when on; sunk paper track when off.
Switch {
    required property var appWindow
    id: sw
    property bool confirmedChecked: false
    checked: confirmedChecked
    onConfirmedCheckedChanged: checked = confirmedChecked
    implicitWidth: 46
    implicitHeight: 26
    hoverEnabled: true

    indicator: Rectangle {
        implicitWidth: 46
        implicitHeight: 26
        radius: height / 2
        color: sw.checked ? Theme.accent : Theme.surfaceSunk
        border.width: 1
        border.color: sw.checked ? Theme.accent : (sw.hovered ? Theme.lineHi : Theme.line)
        Behavior on color { ColorAnimation { duration: Theme.tBase } }
        Behavior on border.color { ColorAnimation { duration: Theme.tBase } }

        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 4
            x: sw.checked ? parent.width - width - 4 : 4
            color: sw.checked ? Theme.accentText : Theme.txtDim
            Behavior on x { NumberAnimation { duration: Theme.duration(200); easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: Theme.tBase } }
        }
    }
    contentItem: Item {}
}
