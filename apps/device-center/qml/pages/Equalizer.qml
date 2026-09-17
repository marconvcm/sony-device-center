import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ViewPage {
    id: root

    readonly property var presets: [0x00, 0x16, 0x15, 0x14, 0x10, 0x11, 0x12, 0x13, 0x17, 0xa0]
    readonly property bool available: controller.connected && controller.hasEqualizer

    function band(i) {
        return controller.equalizerBands && controller.equalizerBands[i] !== undefined ? controller.equalizerBands[i] : 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        anchors.topMargin: 22
        spacing: 16

        // Title and presets share one sheet.
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            implicitHeight: presetCol.implicitHeight + 44
            ColumnLayout {
                id: presetCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 22
                spacing: 16
                SectionTitle { appWindow: root.appWindow;
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    title: appWindow.tr("nav_equalizer")
                    subtitle: appWindow.tr("eq_page_desc")
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: root.presets
                        delegate: PillButton { appWindow: root.appWindow;
                            required property int modelData
                            text: appWindow.trPreset(modelData)
                            active: controller.equalizerPreset === modelData
                            enabled: root.available
                            compact: true
                            onClicked: controller.setEqualizerPreset(modelData)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            // The five bands — the reason anyone opens this screen.
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 3
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Eyebrow { appWindow: root.appWindow; text: appWindow.tr("five_band"); Layout.fillWidth: true }
                        Text { textFormat: Text.PlainText; text: "dB"; color: Theme.txtDim; font.pixelSize: 11 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 0
                        // Scale
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: false
                            Layout.topMargin: 26
                            Layout.bottomMargin: 30
                            Layout.rightMargin: 12
                            Repeater {
                                model: ["+10", "+5", "0", "-5", "-10"]
                                delegate: Text {
                                    required property string modelData
                                    Layout.fillHeight: true
                                    Layout.alignment: Qt.AlignRight
                                    textFormat: Text.PlainText
                                    text: modelData
                                    color: Theme.txtDim
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignTop
                                }
                            }
                        }
                        Repeater {
                            model: ["400", "1k", "2.5k", "6.3k", "16k"]
                            delegate: BandSlider { appWindow: root.appWindow;
                                required property string modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                label: modelData
                                enabled: root.available
                                value: root.band(index)
                                onMoved: function(v) {
                                    var next = []
                                    for (var i = 0; i < 5; ++i) next.push(i === index ? Math.round(v) : root.band(i))
                                    controller.setEqualizerCustom(controller.clearBass, next)
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                spacing: 16

                // Clear Bass
                Card { appWindow: root.appWindow;
                    Layout.fillWidth: true
                    implicitHeight: bassCol.implicitHeight + 44
                    visible: controller.hasClearBass
                    ColumnLayout {
                        id: bassCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 22
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("clear_bass"); Layout.fillWidth: true }
                            Text { textFormat: Text.PlainText; text: "dB"; color: Theme.txtDim; font.pixelSize: 11 }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 18
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                NeoSlider { id: bassSlider; appWindow: root.appWindow;
                                    Layout.fillWidth: true
                                    from: -10; to: 10; stepSize: 1
                                    confirmedValue: controller.clearBass
                                    enabled: root.available
                                    onMoved: controller.setEqualizerCustom(Math.round(value), controller.equalizerBands)
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { textFormat: Text.PlainText; text: "-10"; color: Theme.txtDim; font.pixelSize: 11 }
                                    Item { Layout.fillWidth: true }
                                    Text { textFormat: Text.PlainText; text: "+10"; color: Theme.txtDim; font.pixelSize: 11 }
                                }
                            }
                            DotValue {
                                objectName: "clearBassValue"
                                Layout.alignment: Qt.AlignTop
                                value: Math.round(bassSlider.value)
                                from: -10; to: 10
                                signed: true
                                dot: 5
                                enabled: root.available
                                onEdited: function(v) { controller.setEqualizerCustom(v, controller.equalizerBands) }
                            }
                        }
                    }
                }

                // Active preset
                Card { appWindow: root.appWindow;
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 14
                        Eyebrow { appWindow: root.appWindow; text: appWindow.tr("active_preset") }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            Rectangle {
                                width: 52; height: 52; radius: 26
                                color: Theme.surfaceHi
                                border.width: 1
                                border.color: Theme.line
                                Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.waveform; size: 22; color: Theme.txt; weight: 1.7 }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: root.available ? appWindow.trPreset(controller.equalizerPreset) : "—"
                                    color: Theme.txt
                                    font.pixelSize: 20
                                    font.weight: Font.Bold
                                    font.letterSpacing: -0.4
                                    elide: Text.ElideRight
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: root.available ? appWindow.tr("eq_bands_hint") : appWindow.tr("state_unknown_waiting")
                                    color: Theme.txtDim
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                        // The curve, in numbers.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: 5
                                delegate: Rectangle {
                                    required property int index
                                    Layout.fillWidth: true
                                    height: 36
                                    radius: Theme.controlRadius
                                    color: Theme.surfaceHi
                                    border.width: 1
                                    border.color: Theme.line
                                    Text {
                                        anchors.centerIn: parent
                                        textFormat: Text.PlainText
                                        text: (root.band(index) > 0 ? "+" : "") + root.band(index)
                                        color: root.band(index) === 0 ? Theme.txtDim : Theme.txt
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
}
