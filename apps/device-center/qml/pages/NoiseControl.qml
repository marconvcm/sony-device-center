import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ViewPage {
    id: root

    readonly property var modes: [
        { mode: "cancelling", label: appWindow.tr("noise_cancelling"),  glyph: appWindow.icons.shield,  desc: appWindow.tr("nc_card_cancelling"), available: controller.hasAnc },
        { mode: "ambient",    label: appWindow.tr("ambient_sound"),     glyph: appWindow.icons.ambient, desc: appWindow.tr("nc_card_ambient"),    available: controller.hasAmbient },
        { mode: "off",        label: appWindow.tr("noise_control_off"), glyph: appWindow.icons.power,   desc: appWindow.tr("nc_card_off"),        available: true }
    ]
    readonly property int currentIndex: controller.noiseControlMode === "cancelling" ? 0 : controller.noiseControlMode === "ambient" ? 1 : controller.noiseControlMode === "off" ? 2 : -1

    function apply(mode) {
        if (mode === "cancelling") controller.setAnc(true)
        else if (mode === "ambient") controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
        else controller.setNoiseControlOff()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        anchors.topMargin: 22
        spacing: 16

        SectionTitle { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: false
            eyebrow: appWindow.tr("isolation")
            title: appWindow.tr("nav_noise_control")
            subtitle: appWindow.tr("nc_page_desc")
        }

        // Three-way switch. One ink segment, the rest paper.
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4
                Repeater {
                    model: root.modes
                    delegate: Rectangle {
                        id: segment
                        required property var modelData
                        required property int index
                        readonly property bool current: root.currentIndex === index
                        visible: modelData.available
                        enabled: controller.connected
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Theme.controlRadius
                        color: current ? Theme.accent : segHover.hovered ? Theme.surfaceHi : "transparent"
                        opacity: enabled ? 1 : 0.5
                        Behavior on color { ColorAnimation { duration: Theme.tBase } }
                        HoverHandler { id: segHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: root.apply(segment.modelData.mode) }
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 10
                            Glyph { appWindow: root.appWindow; path: segment.modelData.glyph; size: 20; weight: 1.7; color: segment.current ? Theme.accentText : Theme.txt }
                            Text {
                                textFormat: Text.PlainText
                                text: segment.modelData.label
                                color: segment.current ? Theme.accentText : Theme.txt
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                Behavior on color { ColorAnimation { duration: Theme.tFast } }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            // Ambient level
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                visible: controller.hasAmbient
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 14
                    RowLayout {
                        Layout.fillWidth: true
                        Eyebrow { appWindow: root.appWindow; text: appWindow.tr("ambient_level"); Layout.fillWidth: true }
                        DotValue {
                            objectName: "ambientLevelValue"
                            value: Math.round(levelSlider.value)
                            from: 1; to: 20
                            dot: 4.5
                            enabled: controller.connected
                            onEdited: function(v) { controller.setAmbient(v, voiceSwitch.checked) }
                        }
                    }
                    NeoSlider { id: levelSlider; appWindow: root.appWindow;
                        Layout.fillWidth: true
                        from: 1; to: 20; stepSize: 1
                        confirmedValue: controller.ambientLevel
                        enabled: controller.connected
                        onMoved: controller.setAmbient(Math.round(value), voiceSwitch.checked)
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: -8
                        Text { textFormat: Text.PlainText; text: "1"; color: Theme.txtDim; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text { textFormat: Text.PlainText; text: "20"; color: Theme.txtDim; font.pixelSize: 11 }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line; Layout.topMargin: 6 }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        Rectangle {
                            width: 40; height: 40; radius: 20
                            color: Theme.surfaceHi
                            border.width: 1
                            border.color: Theme.line
                            Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.mic; size: 18; color: Theme.txt; weight: 1.7 }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { textFormat: Text.PlainText; text: appWindow.tr("focus_on_voice"); color: Theme.txt; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Text { textFormat: Text.PlainText; text: appWindow.tr("focus_on_voice_desc"); color: Theme.txtDim; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        }
                        NeoSwitch { id: voiceSwitch; appWindow: root.appWindow;
                            confirmedChecked: controller.focusOnVoice
                            enabled: controller.connected
                            onToggled: controller.setAmbient(controller.ambientLevel, checked)
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            // What the current mode does
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 14
                    Eyebrow { appWindow: root.appWindow; text: appWindow.tr("current_mode") }
                    Rectangle {
                        width: 72; height: 72; radius: 16
                        color: Theme.surfaceHi
                        border.width: 1
                        border.color: Theme.line
                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: root.currentIndex < 0 ? appWindow.icons.info : root.modes[root.currentIndex].glyph
                            size: 34; weight: 1.5; color: Theme.txt
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: root.currentIndex < 0 ? appWindow.tr("unknown") : root.modes[root.currentIndex].label
                        color: Theme.txt
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        font.letterSpacing: -0.4
                    }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: root.currentIndex < 0 ? appWindow.tr("state_unknown_waiting") : root.modes[root.currentIndex].desc
                        color: Theme.txtDim
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
