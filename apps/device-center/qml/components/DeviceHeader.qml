import QtQuick
import ".."
import QtQuick.Layouts

// The strip every page shares: which device, and its three vital signs.
RowLayout {
    required property var appWindow
    id: header
    spacing: 12

    readonly property string modeLabel: !controller.connected || controller.noiseControlMode === "unknown" ? "—"
        : controller.noiseControlMode === "cancelling" ? appWindow.tr("mode_anc")
        : controller.noiseControlMode === "ambient" ? appWindow.tr("mode_ambient") : appWindow.tr("mode_off")
    readonly property string batteryLabel: !controller.connected ? "—"
        : controller.hasDualBattery ? Math.min(controller.batteryLeft < 0 ? 100 : controller.batteryLeft,
                                               controller.batteryRight < 0 ? 100 : controller.batteryRight) + "%"
        : controller.batteryLevel >= 0 ? controller.batteryLevel + "%" : "—"

    ColumnLayout {
        id: nameColumn
        spacing: 6
        Layout.fillWidth: true
        // An explicit floor: otherwise the dot name's natural width becomes
        // the layout minimum and the chips push past the window edge.
        Layout.minimumWidth: 220
        Eyebrow { appWindow: header.appWindow; text: controller.connected ? appWindow.tr("connected_device") : appWindow.tr("offline") }
        DotText {
            objectName: "deviceNameDots"
            text: controller.deviceName
            dot: 6
            maxWidth: nameColumn.width
            color: controller.connected ? Theme.txt : Theme.txtFaint
            Behavior on color { ColorAnimation { duration: Theme.tBase } }
        }
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: controller.hasDualBattery ? appWindow.tr("wireless_earbuds") : appWindow.tr("wireless_headphones")
            color: Theme.txtDim
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }

    Repeater {
        model: [
            { k: appWindow.tr("codec"),      v: controller.connected && controller.codec.length ? controller.codec : "—", g: appWindow.icons.waveform },
            { k: appWindow.tr("battery"),    v: header.batteryLabel, g: controller.isCharging ? appWindow.icons.bolt : appWindow.icons.batteryUp },
            { k: appWindow.tr("sound_mode"), v: header.modeLabel, g: appWindow.icons.ambient }
        ]
        delegate: Card {
            id: chip
            appWindow: header.appWindow
            required property var modelData
            required property int index
            implicitWidth: chipRow.implicitWidth + 32
            implicitHeight: 66
            Layout.alignment: Qt.AlignTop
            RowLayout {
                id: chipRow
                anchors.centerIn: parent
                spacing: 12
                Glyph { appWindow: header.appWindow; path: chip.modelData.g; size: 20; color: Theme.txt; weight: 1.6 }
                ColumnLayout {
                    spacing: 5
                    Eyebrow { appWindow: header.appWindow; text: chip.modelData.k }
                    DotText { text: chip.modelData.v; dot: 3; maxWidth: 120; color: Theme.txt; delay: chip.index * 120 }
                }
            }
        }
    }
}
