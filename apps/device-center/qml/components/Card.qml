import QtQuick
import ".."

// Flat card: a sheet of paper with a hairline edge. No gradients, no glow.
Rectangle {
    required property var appWindow
    id: card
    property bool inverse: false
    property bool active: false
    property bool hovered: false
    radius: Theme.cardRadius
    border.width: 1
    border.color: active ? Theme.accent : hovered ? Theme.lineHi : inverse ? Theme.sidebarLine : Theme.line
    color: inverse ? Theme.sidebarSurface : hovered ? Theme.surfaceHi : Theme.surface
    Behavior on border.color { ColorAnimation { duration: Theme.tBase } }
    Behavior on color { ColorAnimation { duration: Theme.tBase } }
}
