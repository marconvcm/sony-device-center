import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ViewPage {
    id: root

    function availability(key) { return controller.featureStatus[key] }
    function known(key) { var a = availability(key); return controller.connected && a && a.availability === "valid" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        anchors.topMargin: 22
        spacing: 16

        SectionTitle { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: false
            title: appWindow.tr("features_title")
            subtitle: appWindow.tr("features_desc")
        }

        // Headline feature: the upscaler gets the wide row.
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: 108
            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 20
                Rectangle {
                    width: 72; height: 72; radius: 14
                    color: Theme.surfaceHi
                    border.width: 1
                    border.color: Theme.line
                    Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.sparkle; size: 32; weight: 1.5; color: Theme.txt }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Eyebrow { appWindow: root.appWindow; text: appWindow.tr("upscaling") }
                    Text { textFormat: Text.PlainText; text: "DSEE Extreme"; color: Theme.txt; font.pixelSize: 22; font.weight: Font.Bold; font.letterSpacing: -0.4 }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: !controller.hasDsee ? appWindow.tr("not_supported") : !root.known("dsee") ? appWindow.tr("state_unknown_waiting") : appWindow.tr("feat_dsee_desc")
                        color: Theme.txtDim
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }
                Rectangle { width: 1; Layout.fillHeight: true; color: Theme.line }
                RowLayout {
                    spacing: 14
                    Text { textFormat: Text.PlainText; text: controller.dsee ? appWindow.tr("active") : appWindow.tr("noise_control_off"); color: Theme.txt; font.pixelSize: 13; font.weight: Font.Medium }
                    NeoSwitch { appWindow: root.appWindow;
                        enabled: controller.hasDsee && controller.connected
                        confirmedChecked: controller.dsee
                        onToggled: controller.setDsee(checked)
                    }
                }
            }
        }

        // Feature tiles
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: 168
            spacing: 16
            Repeater {
                model: [
                    { key: "speakToChat",   title: "Speak-to-Chat",   desc: appWindow.tr("feat_speak_desc"),    glyph: appWindow.icons.chat,   supported: controller.hasSpeakToChat,   on: controller.speakToChat },
                    { key: "adaptiveVolume", title: "Adaptive Volume", desc: appWindow.tr("feat_adaptive_desc"), glyph: appWindow.icons.volume, supported: controller.hasAdaptiveVolume, on: controller.adaptiveVolume }
                ]
                delegate: Card { appWindow: root.appWindow;
                    id: tile
                    required property var modelData
                    readonly property bool ready: modelData.supported && root.known(modelData.key)
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 8
                        Glyph { appWindow: root.appWindow; path: tile.modelData.glyph; size: 26; weight: 1.6; color: Theme.txt }
                        Text { textFormat: Text.PlainText; Layout.topMargin: 4; text: tile.modelData.title; color: Theme.txt; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: !tile.modelData.supported ? appWindow.tr("not_supported") : !tile.ready ? appWindow.tr("state_unknown_waiting") : tile.modelData.desc
                            color: Theme.txtDim
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        Item { Layout.fillHeight: true }
                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: tile.modelData.on ? appWindow.tr("active") : appWindow.tr("noise_control_off"); color: Theme.txt; font.pixelSize: 12; font.weight: Font.Medium }
                            NeoSwitch { appWindow: root.appWindow;
                                enabled: tile.modelData.supported && controller.connected
                                confirmedChecked: tile.modelData.on
                                onToggled: tile.modelData.key === "speakToChat" ? controller.setSpeakToChat(checked) : controller.setAdaptiveVolume(checked)
                            }
                        }
                    }
                }
            }
        }

        // Auto power off: the wide row with a picker on the right.
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 24
                    color: Theme.surfaceHi
                    border.width: 1
                    border.color: Theme.line
                    Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.clock; size: 22; weight: 1.6; color: Theme.txt }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { textFormat: Text.PlainText; text: appWindow.tr("auto_power_off"); color: Theme.txt; font.pixelSize: 15; font.weight: Font.DemiBold }
                    Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: appWindow.tr("auto_power_off_desc"); color: Theme.txtDim; font.pixelSize: 12; elide: Text.ElideRight }
                }
                ComboBox {
                    id: powerCombo
                    implicitWidth: 150
                    implicitHeight: 40
                    enabled: controller.connected
                    model: [appWindow.tr("apo_off"), appWindow.tr("apo_5min"), appWindow.tr("apo_15min"), appWindow.tr("apo_30min"), appWindow.tr("apo_1h"), appWindow.tr("apo_3h")]
                    currentIndex: root.known("autoPowerOff") ? controller.autoPowerOff : -1
                    Connections {
                        target: controller
                        function onStateChanged() { powerCombo.currentIndex = Qt.binding(function() { return root.known("autoPowerOff") ? controller.autoPowerOff : -1 }) }
                    }
                    onActivated: controller.setAutoPowerOff(index)

                    background: Rectangle {
                        radius: Theme.controlRadius
                        color: powerCombo.hovered ? Theme.surfaceHi : Theme.surface
                        border.width: 1
                        border.color: powerCombo.hovered ? Theme.lineHi : Theme.line
                        Behavior on color { ColorAnimation { duration: Theme.tFast } }
                    }
                    contentItem: Text {
                        textFormat: Text.PlainText
                        leftPadding: 14
                        rightPadding: 30
                        text: powerCombo.displayText
                        color: Theme.txt
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    indicator: Glyph { appWindow: root.appWindow;
                        x: powerCombo.width - width - 12
                        y: powerCombo.height / 2 - height / 2
                        size: 14
                        color: Theme.txtDim
                        path: appWindow.icons.chevron
                        rotation: powerCombo.popup.visible ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: Theme.tBase } }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
