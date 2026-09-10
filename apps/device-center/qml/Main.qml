import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1060
    height: 720
    minimumWidth: 880
    minimumHeight: 620
    visible: true
    title: "Sony Device Center — " + controller.deviceName
    color: "#0f1015"

    // Modern Palette
    readonly property color bgDark: "#0f1015"
    readonly property color bgCard: "#181a24"
    readonly property color bgCardHover: "#202331"
    readonly property color borderCard: "#272a38"
    readonly property color accentPurple: "#8b5cf6"
    readonly property color accentHover: "#a78bfa"
    readonly property color textPrimary: "#f8fafc"
    readonly property color textSecondary: "#94a3b8"
    readonly property color textMuted: "#64748b"
    readonly property color greenSuccess: "#10b981"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // LEFT SIDEBAR: Navigation & Device Status
        // ==========================================
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 240
            color: "#13141c"
            border.color: borderCard
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 24

                // App Brand
                RowLayout {
                    spacing: 12
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 10
                        color: accentPurple
                        Text {
                            anchors.centerIn: parent
                            text: "S"
                            color: "white"
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: "Device Center"
                            color: textPrimary
                            font.pixelSize: 16
                            font.bold: true
                        }
                        Text {
                            text: "Sony Audio Companion"
                            color: textMuted
                            font.pixelSize: 11
                        }
                    }
                }

                // Active Device Summary Badge
                Rectangle {
                    Layout.fillWidth: true
                    height: 70
                    radius: 12
                    color: bgCard
                    border.color: borderCard
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: controller.connected ? greenSuccess : "#ef4444"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: controller.deviceName
                                color: textPrimary
                                font.pixelSize: 14
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: controller.connected ? ("Battery " + controller.batteryLevel + "%" + (controller.isCharging ? " ⚡" : "")) : "Disconnected"
                                color: controller.connected ? textSecondary : textMuted
                                font.pixelSize: 12
                            }
                        }
                    }
                }

                // Nav Links
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: [
                            { id: 0, name: "Overview", icon: "🎧" },
                            { id: 1, name: "Noise Control", icon: "🛡️" },
                            { id: 2, name: "Equalizer", icon: "🎚️" },
                            { id: 3, name: "Audio Features", icon: "✨" },
                            { id: 4, name: "Device Switcher", icon: "🔄" }
                        ]

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 42
                            radius: 10
                            color: navIndex === modelData.id ? accentPurple : (navHover.hovered ? bgCardHover : "transparent")

                            HoverHandler { id: navHover }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 12

                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 14
                                }

                                Text {
                                    text: modelData.name
                                    color: navIndex === modelData.id ? "white" : (navHover.hovered ? textPrimary : textSecondary)
                                    font.pixelSize: 13
                                    font.bold: navIndex === modelData.id
                                    Layout.fillWidth: true
                                }
                            }

                            TapHandler {
                                onTapped: navIndex = modelData.id
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Daemon Status indicator
                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    radius: 8
                    color: "#1c1e2b"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: greenSuccess
                        }
                        Text {
                            text: "SDK Core / IPC Connected"
                            color: textMuted
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        // ==========================================
        // MAIN CONTENT AREA
        // ==========================================
        StackLayout {
            id: mainStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navIndex

            // ------------------------------------------
            // 1. OVERVIEW VIEW (Hero visualization)
            // ------------------------------------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 40
                    spacing: 24

                    // Header
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            spacing: 4
                            Text {
                                text: controller.deviceName
                                color: textPrimary
                                font.pixelSize: 28
                                font.bold: true
                            }
                            Text {
                                text: "Bluetooth RFCOMM • Protocol V2 • " + (controller.connected ? "Active" : "Offline")
                                color: textSecondary
                                font.pixelSize: 13
                            }
                        }
                        Item { Layout.fillWidth: true }
                        // Battery Capsule Pill
                        Rectangle {
                            height: 36
                            width: 140
                            radius: 18
                            color: bgCard
                            border.color: borderCard
                            border.width: 1
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 8
                                Text { text: "🔋"; font.pixelSize: 14 }
                                Text {
                                    text: controller.batteryLevel + "%"
                                    color: textPrimary
                                    font.bold: true
                                    font.pixelSize: 14
                                }
                                Text {
                                    text: controller.isCharging ? "Charging" : "Ready"
                                    color: controller.isCharging ? greenSuccess : textSecondary
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    // Hero Visualization Card
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 20
                        color: bgCard
                        border.color: borderCard
                        border.width: 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 20

                            Image {
                                id: heroImage
                                Layout.preferredWidth: 320
                                Layout.preferredHeight: 320
                                fillMode: Image.PreserveAspectFit
                                source: "../" + controller.heroImagePath
                                opacity: controller.connected ? 1.0 : 0.4
                                scale: heroHover.hovered ? 1.05 : 1.0

                                Behavior on scale {
                                    NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
                                }

                                HoverHandler { id: heroHover }
                            }

                            // Quick Action Bar beneath hero
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 14

                                Button {
                                    text: controller.noiseControlMode === "cancelling" ? "ANC: Active" : "Enable ANC"
                                    highlighted: controller.noiseControlMode === "cancelling"
                                    onClicked: controller.setAnc(controller.noiseControlMode !== "cancelling")
                                }

                                Button {
                                    text: controller.noiseControlMode === "ambient" ? "Ambient: Active" : "Ambient Sound"
                                    highlighted: controller.noiseControlMode === "ambient"
                                    onClicked: controller.setAmbient(10, false)
                                }

                                Button {
                                    text: "Equalizer: " + controller.equalizerPresetName
                                    onClicked: navIndex = 2
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------
            // 2. NOISE CONTROL VIEW
            // ------------------------------------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 40
                    spacing: 28

                    ColumnLayout {
                        spacing: 4
                        Text {
                            text: "Noise Control"
                            color: textPrimary
                            font.pixelSize: 26
                            font.bold: true
                        }
                        Text {
                            text: "Tune your sound isolation with active noise cancelling and ambient sound."
                            color: textSecondary
                            font.pixelSize: 13
                        }
                    }

                    // Segmented Mode Selector Card
                    Rectangle {
                        Layout.fillWidth: true
                        height: 90
                        radius: 16
                        color: bgCard
                        border.color: borderCard
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Repeater {
                                model: [
                                    { mode: "cancelling", label: "Noise Cancelling", icon: "🛡️", desc: "Block outside ambient noise" },
                                    { mode: "ambient", label: "Ambient Sound", icon: "🎙️", desc: "Hear your environment" },
                                    { mode: "off", label: "Off", icon: "⭕", desc: "Disable all sound processing" }
                                ]

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 12
                                    color: controller.noiseControlMode === modelData.mode ? accentPurple : (modeHover.hovered ? bgCardHover : "#1c1e2b")
                                    border.color: controller.noiseControlMode === modelData.mode ? accentPurple : borderCard

                                    HoverHandler { id: modeHover }

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 10
                                        Text { text: modelData.icon; font.pixelSize: 18 }
                                        ColumnLayout {
                                            spacing: 1
                                            Text {
                                                text: modelData.label
                                                color: textPrimary
                                                font.pixelSize: 14
                                                font.bold: true
                                            }
                                            Text {
                                                text: modelData.desc
                                                color: controller.noiseControlMode === modelData.mode ? "#e2e8f0" : textMuted
                                                font.pixelSize: 11
                                            }
                                        }
                                    }

                                    TapHandler {
                                        onTapped: {
                                            if (modelData.mode === "cancelling") controller.setAnc(true)
                                            else if (modelData.mode === "ambient") controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
                                            else controller.setNoiseControlOff()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Ambient Sound Level Card
                    Rectangle {
                        Layout.fillWidth: true
                        height: 150
                        radius: 16
                        color: bgCard
                        border.color: borderCard
                        border.width: 1
                        opacity: controller.noiseControlMode === "ambient" ? 1.0 : 0.4
                        enabled: controller.noiseControlMode === "ambient"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 16

                            RowLayout {
                                Text {
                                    text: "Ambient Sound Level"
                                    color: textPrimary
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    width: 48
                                    height: 28
                                    radius: 6
                                    color: accentPurple
                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(ambientSlider.value)
                                        color: "white"
                                        font.bold: true
                                        font.pixelSize: 13
                                    }
                                }
                            }

                            Slider {
                                id: ambientSlider
                                Layout.fillWidth: true
                                from: 1
                                to: 20
                                stepSize: 1
                                value: controller.ambientLevel
                                onMoved: controller.setAmbient(Math.round(value), voiceSwitch.checked)
                            }

                            RowLayout {
                                spacing: 10
                                Switch {
                                    id: voiceSwitch
                                    checked: controller.focusOnVoice
                                    onToggled: controller.setAmbient(controller.ambientLevel, checked)
                                }
                                Text {
                                    text: "Focus on Voice (enhance human speech while filtering low frequencies)"
                                    color: textSecondary
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ------------------------------------------
            // 3. EQUALIZER VIEW
            // ------------------------------------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 40
                    spacing: 24

                    ColumnLayout {
                        spacing: 4
                        Text {
                            text: "Equalizer"
                            color: textPrimary
                            font.pixelSize: 26
                            font.bold: true
                        }
                        Text {
                            text: "Sculpt your audio signature across 5 bands and dedicated Clear Bass enhancement."
                            color: textSecondary
                            font.pixelSize: 13
                        }
                    }

                    // Preset Chips Flow
                    Rectangle {
                        Layout.fillWidth: true
                        height: 70
                        radius: 16
                        color: bgCard
                        border.color: borderCard
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Repeater {
                                model: [
                                    { id: 0x00, name: "Off" },
                                    { id: 0x16, name: "Bass Boost" },
                                    { id: 0x15, name: "Treble Boost" },
                                    { id: 0x14, name: "Vocal" },
                                    { id: 0x10, name: "Bright" },
                                    { id: 0x11, name: "Excited" },
                                    { id: 0x12, name: "Mellow" },
                                    { id: 0x13, name: "Relaxed" },
                                    { id: 0x17, name: "Speech" },
                                    { id: 0xa0, name: "Custom" }
                                ]

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 10
                                    color: controller.equalizerPreset === modelData.id ? accentPurple : (chipHover.hovered ? bgCardHover : "#1c1e2b")

                                    HoverHandler { id: chipHover }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.name
                                        color: controller.equalizerPreset === modelData.id ? "white" : textSecondary
                                        font.pixelSize: 12
                                        font.bold: controller.equalizerPreset === modelData.id
                                    }

                                    TapHandler {
                                        onTapped: controller.setEqualizerPreset(modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    // Clear Bass Card
                    Rectangle {
                        Layout.fillWidth: true
                        height: 90
                        radius: 16
                        color: bgCard
                        border.color: borderCard
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 20

                            ColumnLayout {
                                spacing: 2
                                Text { text: "Clear Bass"; color: textPrimary; font.bold: true; font.pixelSize: 15 }
                                Text { text: "Deep sub-bass response without distortion"; color: textMuted; font.pixelSize: 11 }
                            }

                            Slider {
                                id: clearBassSlider
                                Layout.fillWidth: true
                                from: -10
                                to: 10
                                stepSize: 1
                                value: controller.clearBass
                                onMoved: controller.setEqualizerCustom(Math.round(value), controller.equalizerBands)
                            }

                            Rectangle {
                                width: 44
                                height: 28
                                radius: 6
                                color: accentPurple
                                Text {
                                    anchors.centerIn: parent
                                    text: (controller.clearBass > 0 ? "+" : "") + controller.clearBass
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ------------------------------------------
            // 4. AUDIO FEATURES VIEW
            // ------------------------------------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 40
                    spacing: 24

                    ColumnLayout {
                        spacing: 4
                        Text {
                            text: "Sound & Device Features"
                            color: textPrimary
                            font.pixelSize: 26
                            font.bold: true
                        }
                        Text {
                            text: "Configure AI audio upscaling, speak-to-chat, and intelligent battery saving."
                            color: textSecondary
                            font.pixelSize: 13
                        }
                    }

                    // Feature Cards Grid
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 16
                        columnSpacing: 16

                        // DSEE Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: 100
                            radius: 16
                            color: bgCard
                            border.color: borderCard

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text { text: "DSEE Extreme"; color: textPrimary; font.bold: true; font.pixelSize: 15 }
                                    Text { text: "AI restores high frequencies lost during compression"; color: textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                }
                                Switch {
                                    checked: controller.dsee
                                    onToggled: controller.setDsee(checked)
                                }
                            }
                        }

                        // Speak to Chat Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: 100
                            radius: 16
                            color: bgCard
                            border.color: borderCard

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text { text: "Speak-to-Chat"; color: textPrimary; font.bold: true; font.pixelSize: 15 }
                                    Text { text: "Pauses music and lets ambient sound in when you speak"; color: textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                }
                                Switch {
                                    checked: controller.speakToChat
                                    onToggled: controller.setSpeakToChat(checked)
                                }
                            }
                        }

                        // Adaptive Volume Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: 100
                            radius: 16
                            color: bgCard
                            border.color: borderCard

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text { text: "Adaptive Volume"; color: textPrimary; font.bold: true; font.pixelSize: 15 }
                                    Text { text: "Dynamically balances volume matching surroundings"; color: textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                }
                                Switch {
                                    checked: controller.adaptiveVolume
                                    onToggled: controller.setAdaptiveVolume(checked)
                                }
                            }
                        }

                        // Auto Power Off Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: 100
                            radius: 16
                            color: bgCard
                            border.color: borderCard

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text { text: "Auto Power-Off"; color: textPrimary; font.bold: true; font.pixelSize: 15 }
                                    Text { text: "Turn off automatically when headphones are removed"; color: textMuted; font.pixelSize: 12 }
                                }
                                ComboBox {
                                    model: ["Off", "5 Minutes", "15 Minutes", "30 Minutes", "1 Hour", "3 Hours"]
                                    currentIndex: controller.autoPowerOff
                                    onActivated: controller.setAutoPowerOff(index)
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ------------------------------------------
            // 5. DEVICE SWITCHER VIEW (Easy Switch reference)
            // ------------------------------------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 40
                    spacing: 24

                    ColumnLayout {
                        spacing: 4
                        Text {
                            text: "Paired Sony Devices"
                            color: textPrimary
                            font.pixelSize: 26
                            font.bold: true
                        }
                        Text {
                            text: "Seamlessly switch host connection between paired headphones and earbuds."
                            color: textSecondary
                            font.pixelSize: 13
                        }
                    }

                    // Device List Repeater
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Repeater {
                            model: controller.pairedDevices

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                height: 75
                                radius: 16
                                color: modelData.name === controller.deviceName ? "#212433" : bgCard
                                border.color: modelData.name === controller.deviceName ? accentPurple : borderCard
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 16

                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: modelData.name === controller.deviceName ? greenSuccess : textMuted
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: modelData.name
                                            color: textPrimary
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                        Text {
                                            text: modelData.address + (modelData.name === controller.deviceName ? " • Connected" : " • Available")
                                            color: modelData.name === controller.deviceName ? greenSuccess : textMuted
                                            font.pixelSize: 12
                                        }
                                    }

                                    Button {
                                        text: modelData.name === controller.deviceName ? "Active" : "Connect"
                                        highlighted: modelData.name === controller.deviceName
                                        enabled: modelData.name !== controller.deviceName
                                        onClicked: controller.connectDevice(modelData.address, modelData.name)
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    property int navIndex: 0
}
