import QtQuick
import ".."

// The mark: twenty dots tracing an S, taken from assets/mark.svg.
Item {
    required property var appWindow
    id: brandMark
    property real size: 24
    property color color: "#FFFFFF"
    implicitWidth: size
    implicitHeight: size

    readonly property var dots: [
        [0.815, 0.063], [0.447, 0.065], [0.630, 0.065], [0.265, 0.067],
        [0.091, 0.209], [0.265, 0.209], [0.091, 0.361], [0.265, 0.361],
        [0.783, 0.502], [0.257, 0.505], [0.435, 0.505], [0.610, 0.505],
        [0.759, 0.646], [0.909, 0.646], [0.757, 0.793], [0.909, 0.793],
        [0.255, 0.935], [0.428, 0.935], [0.601, 0.935], [0.760, 0.937]
    ]
    Repeater {
        model: brandMark.dots
        Rectangle {
            required property var modelData
            readonly property real r: brandMark.size * 0.0634
            x: modelData[0] * brandMark.size - r
            y: modelData[1] * brandMark.size - r
            width: r * 2
            height: r * 2
            radius: r
            color: brandMark.color
            antialiasing: true
        }
    }
}
