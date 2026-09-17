import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Every view enters the same way. Consistency is the whole point.
Item {
    required property var appWindow
    id: page
    default property alias pageData: inner.data

    Item {
        id: inner
        width: parent.width
        height: parent.height
        opacity: 0
    }

    ParallelAnimation {
        id: entrance
        NumberAnimation { target: inner; property: "opacity"; from: 0; to: 1; duration: Theme.duration(260); easing.type: Easing.OutQuad }
        NumberAnimation { target: inner; property: "y"; from: 16; to: 0; duration: Theme.tSlow; easing.type: Easing.OutCubic }
    }

    Connections {
        target: Theme
        function onMotionEnabledChanged() {
            if (!Theme.motionEnabled) {
                entrance.stop()
                inner.opacity = 1
                inner.y = 0
            }
        }
    }
    onVisibleChanged: if (visible) entrance.restart()
    Component.onCompleted: if (visible) entrance.restart()
}
