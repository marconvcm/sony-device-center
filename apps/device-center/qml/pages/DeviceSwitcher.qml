import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ViewPage {
    id: root

    function isCurrent(device) {
        // The address is what the connection was made with; the name is
        // only a fallback for snapshots that carry no address yet.
        return controller.deviceAddress !== "" ? device.address === controller.deviceAddress
                                                : device.name === controller.deviceName
    }
    readonly property var current: controller.connected ? controller.pairedDevices.filter(d => root.isCurrent(d)) : []
    readonly property var others: controller.pairedDevices.filter(d => !(controller.connected && root.isCurrent(d)))

    // Every row's button is as wide as the widest label the column can
    // show, so the buttons line up down the list in every language and
    // never clip; the probes are measured, never shown.
    readonly property real actionWidth: Math.max(120, activeProbe.implicitWidth, connectProbe.implicitWidth)
    PillButton { id: activeProbe; appWindow: root.appWindow; visible: false; text: appWindow.tr("active") }
    PillButton { id: connectProbe; appWindow: root.appWindow; visible: false; text: appWindow.tr("connect"); glyphPath: appWindow.icons.swap }

    // A long list of paired sets runs past the minimum window height, so
    // the page scrolls like Settings does.
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: devicesColumn.implicitHeight + 54
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: devicesColumn
        x: 32; y: 22
        width: parent.width - 64
        spacing: 22

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            SectionTitle { appWindow: root.appWindow;
                Layout.fillWidth: true
                eyebrow: appWindow.tr("easy_switch")
                title: appWindow.tr("paired_devices")
                subtitle: appWindow.tr("paired_devices_desc")
            }
            PillButton { appWindow: root.appWindow;
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 18
                compact: true
                text: appWindow.tr("devices_rescan")
                glyphPath: appWindow.icons.refresh
                onClicked: controller.refreshDiscoveredDevices()
            }
        }

        // The headphones this window is talking to.
        ColumnLayout {
            visible: root.current.length > 0
            Layout.fillWidth: true
            spacing: 10
            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("devices_current") }
            Repeater {
                model: root.current
                delegate: DeviceRow { appWindow: root.appWindow; current: true }
            }
        }

        // Everything else paired with this computer.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            Eyebrow { appWindow: root.appWindow; visible: root.current.length > 0; text: appWindow.tr("devices_others") }
            Repeater {
                model: root.others
                delegate: DeviceRow { appWindow: root.appWindow }
            }
            // Only the connected pair is known: one quiet line.
            Text {
                visible: root.others.length === 0 && root.current.length > 0
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: appWindow.tr("devices_none_others")
                color: Theme.txtFaint
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
            // Nothing paired at all.
            Card { appWindow: root.appWindow;
                visible: controller.pairedDevices.length === 0
                Layout.fillWidth: true
                implicitHeight: emptyColumn.implicitHeight + 56
                ColumnLayout {
                    id: emptyColumn
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 6
                    Rectangle {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        Layout.bottomMargin: 8
                        radius: Theme.cardRadius
                        color: Theme.surfaceSunk
                        border.width: 1
                        border.color: Theme.line
                        Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.headphones; size: 22; color: Theme.txtFaint }
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: appWindow.tr("devices_none_title")
                        color: Theme.txt
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: appWindow.tr("devices_none_desc")
                        color: Theme.txtDim
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
    }

    // One paired device: icon tile, name and status, and the action column.
    component DeviceRow: Card {
        id: devCard
        required property var modelData
        property bool current: false

        Layout.fillWidth: true
        Layout.preferredHeight: 84
        active: current
        hovered: devHover.hovered && !current
        scale: hovered ? 1.006 : 1.0
        Behavior on scale { NumberAnimation { duration: Theme.tBase } }

        HoverHandler { id: devHover }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            Rectangle {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                radius: Theme.cardRadius
                color: devCard.current ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : Theme.surfaceSunk
                border.width: 1
                border.color: devCard.current ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5) : Theme.line

                Glyph { appWindow: devCard.appWindow;
                    anchors.centerIn: parent
                    path: appWindow.icons.headphones
                    size: 22
                    color: devCard.current ? Theme.accentSoft : Theme.txtFaint
                }
            }

            // Fills whatever the action column leaves, and elides rather
            // than pushing the button around when a name is long.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: devCard.modelData.name
                    color: Theme.txt
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7
                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: devCard.current ? Theme.success : Theme.txtFaint
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: devCard.current ? appWindow.tr("connected") : appWindow.tr("available")
                        color: devCard.current ? Theme.success : Theme.txtFaint
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: "·  " + devCard.modelData.address
                        color: Theme.txtFaint
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }

            PillButton { appWindow: devCard.appWindow;
                Layout.preferredWidth: root.actionWidth
                Layout.minimumWidth: root.actionWidth
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: 40
                text: devCard.current ? appWindow.tr("active") : appWindow.tr("connect")
                glyphPath: devCard.current ? "" : appWindow.icons.swap
                active: devCard.current
                enabled: !devCard.current && !controller.busy
                opacity: devCard.current ? 0.8 : enabled ? 1.0 : 0.6
                onClicked: controller.connectDevice(devCard.modelData.address, devCard.modelData.name)
            }
        }
    }
}
