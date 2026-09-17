import QtQuick
import ".."

// The mark on an ink tile.
Rectangle {
    required property var appWindow
    id: brandTile
    radius: width * 0.25
    color: Theme.accent
    BrandMark { appWindow: brandTile.appWindow;
        anchors.centerIn: parent
        size: brandTile.width * 0.6
        color: Theme.accentText
    }
}
