import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Soft radial bloom. Atmosphere only — never carries information.
Shape {
    required property var appWindow
    id: bloom
    property color tint: Theme.accent
    property real strength: 0.22
    antialiasing: true
    ShapePath {
        strokeWidth: -1
        fillGradient: RadialGradient {
            centerX: bloom.width / 2
            centerY: bloom.height / 2
            centerRadius: bloom.width / 2
            focalX: centerX
            focalY: centerY
            GradientStop { position: 0.0; color: Qt.rgba(bloom.tint.r, bloom.tint.g, bloom.tint.b, bloom.strength) }
            GradientStop { position: 0.55; color: Qt.rgba(bloom.tint.r, bloom.tint.g, bloom.tint.b, bloom.strength * 0.35) }
            GradientStop { position: 1.0; color: Qt.rgba(bloom.tint.r, bloom.tint.g, bloom.tint.b, 0.0) }
        }
        startX: 0; startY: 0
        PathLine { x: bloom.width; y: 0 }
        PathLine { x: bloom.width; y: bloom.height }
        PathLine { x: 0; y: bloom.height }
    }
}
