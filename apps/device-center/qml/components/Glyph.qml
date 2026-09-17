import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Stroked SVG glyph. One string may hold several subpaths.
Item {
    required property var appWindow
    id: glyph
    property string path: ""
    property real size: 18
    property color color: Theme.txtDim
    property real weight: 1.8
    implicitWidth: size
    implicitHeight: size

    Shape {
        anchors.fill: parent
        antialiasing: Theme.iconAntialiasing
        // Geometry rendering needs multisampling to smooth ShapePath edges.
        layer.enabled: Theme.iconAntialiasing
        layer.samples: Theme.iconAntialiasing ? 4 : 0
        ShapePath {
            strokeColor: glyph.color
            // Path.scale scales geometry only — strokeWidth is already in
            // item pixels, so pre-multiplying it just makes icons look faint.
            strokeWidth: glyph.weight
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            scale: Qt.size(glyph.size / 24, glyph.size / 24)
            PathSvg { path: glyph.path }
        }
    }
    Behavior on color { ColorAnimation { duration: Theme.tFast } }
}
