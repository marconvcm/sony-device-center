import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ViewPage {
    id: root

    readonly property string modeTitle: !controller.connected || controller.noiseControlMode === "unknown" ? appWindow.tr("unknown")
        : controller.noiseControlMode === "cancelling" ? appWindow.tr("noise_cancelling")
        : controller.noiseControlMode === "ambient" ? appWindow.tr("ambient_sound") : appWindow.tr("nc_title_off")
    readonly property string modeDesc: !controller.connected || controller.noiseControlMode === "unknown" ? appWindow.tr("state_unknown_waiting")
        : controller.noiseControlMode === "cancelling" ? appWindow.tr("nc_desc_cancelling")
        : controller.noiseControlMode === "ambient" ? appWindow.tr("nc_desc_ambient").arg(controller.ambientLevel) : appWindow.tr("nc_desc_off")
    readonly property int batteryPercent: !controller.connected ? -1
        : controller.hasDualBattery ? Math.min(controller.batteryLeft < 0 ? 100 : controller.batteryLeft,
                                               controller.batteryRight < 0 ? 100 : controller.batteryRight)
        : controller.batteryLevel

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        anchors.topMargin: 22
        spacing: 16

        // Hero: the mode you are in, and the thing on your head.
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                id: heroCopy
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 28
                width: Math.max(300, parent.width * 0.5)
                spacing: 0

                Eyebrow { appWindow: root.appWindow; text: appWindow.tr("sound_mode") }
                DotText {
                    objectName: "modeDots"
                    Layout.topMargin: 16
                    text: root.modeTitle
                    dot: 6
                    maxWidth: heroCopy.width
                    color: Theme.txt
                }
                Text {
                    Layout.topMargin: 14
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: root.modeDesc
                    color: Theme.txtDim
                    font.pixelSize: 16
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    spacing: 10
                    PillButton { appWindow: root.appWindow;
                        text: appWindow.tr("noise_cancelling")
                        glyphPath: appWindow.icons.shield
                        visible: controller.hasAnc
                        active: controller.noiseControlMode === "cancelling"
                        enabled: controller.connected
                        onClicked: controller.setAnc(true)
                    }
                    PillButton { appWindow: root.appWindow;
                        text: appWindow.tr("ambient")
                        glyphPath: appWindow.icons.ambient
                        visible: controller.hasAmbient
                        active: controller.noiseControlMode === "ambient"
                        enabled: controller.connected
                        onClicked: controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
                    }
                    PillButton { appWindow: root.appWindow;
                        text: appWindow.tr("noise_control_off")
                        glyphPath: appWindow.icons.power
                        active: controller.noiseControlMode === "off"
                        enabled: controller.connected
                        onClicked: controller.setNoiseControlOff()
                    }
                }
            }

            // Product, framed by a dotted orbit and a pinch of type.
            Item {
                id: stage
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 8
                width: parent.width - heroCopy.width - 56

                Canvas {
                    id: orbit
                    anchors.fill: parent
                    opacity: 0.55
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Connections { target: Theme; function onLightChanged() { orbit.requestPaint() } }
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        var r = Math.min(width, height) * 0.46
                        ctx.strokeStyle = Theme.lineHi
                        ctx.lineWidth = 1.2
                        ctx.setLineDash([2, 5])
                        ctx.beginPath()
                        ctx.arc(width * 0.52, height * 0.5, r, Math.PI * 0.55, Math.PI * 1.75)
                        ctx.stroke()
                    }
                }

                Image {
                    id: product
                    anchors.centerIn: parent
                    width: Math.min(parent.width * 0.7, parent.height * 0.92)
                    height: width
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    source: "qrc:/" + controller.heroImagePath
                    opacity: controller.connected ? 1.0 : 0.4
                    scale: productHover.hovered ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: Theme.duration(320); easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: Theme.tSlow } }
                    HoverHandler { id: productHover }
                }

                // Dot grid, top right.
                Grid {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24
                    columns: 4
                    spacing: 7
                    Repeater {
                        model: 20
                        Rectangle { width: 3; height: 3; radius: 1.5; color: Theme.lineHi }
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 26
                    width: 92
                    textFormat: Text.PlainText
                    text: appWindow.tr("hero_tagline")
                    color: Theme.txtDim
                    font.pixelSize: 9
                    font.capitalization: Font.AllUppercase
                    lineHeight: 1.5
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignLeft
                }
            }
        }

        // Vitals
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: 158
            spacing: 16

            // The three cards share the row 6:5:4 (preferred widths are
            // proportions, in pixels so a minimum can sit beside them).
            // Battery & connection
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 300
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12
                    CardTitle { appWindow: root.appWindow; text: appWindow.tr("battery_connection"); onClicked: appWindow.navIndex = 5 }
                    RowLayout {
                        Layout.fillHeight: true
                        spacing: 18
                        ColumnLayout {
                            id: batteryColumn
                            spacing: 12
                            Layout.preferredWidth: 104
                            Layout.maximumWidth: 104
                            DotText {
                                text: root.batteryPercent >= 0 ? root.batteryPercent + "%" : "—"
                                dot: 4.5
                                maxWidth: batteryColumn.width
                                color: root.batteryPercent >= 0 && root.batteryPercent <= 20 ? Theme.danger : Theme.txt
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 5
                                radius: 2.5
                                color: Theme.surfaceSunk
                                Rectangle {
                                    width: parent.width * Math.max(0, root.batteryPercent) / 100
                                    height: parent.height
                                    radius: parent.radius
                                    color: root.batteryPercent >= 0 && root.batteryPercent <= 20 ? Theme.danger : Theme.accent
                                    Behavior on width { NumberAnimation { duration: Theme.duration(600); easing.type: Easing.OutCubic } }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                        Rectangle { width: 1; Layout.fillHeight: true; color: Theme.line }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            VitalRow { appWindow: root.appWindow;
                                glyph: appWindow.icons.bluetooth
                                title: controller.connected ? appWindow.tr("connected") : appWindow.tr("disconnected")
                                detail: controller.connected ? appWindow.tr("stable_connection") : appWindow.trState(controller.connectionState)
                            }
                            VitalRow { appWindow: root.appWindow;
                                glyph: appWindow.icons.waveform
                                title: controller.connected && controller.codec.length ? controller.codec : "—"
                                detail: appWindow.tr("high_quality_audio")
                            }
                        }
                    }
                }
            }

            // Battery history
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 250
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    anchors.topMargin: 16
                    anchors.bottomMargin: 14
                    spacing: 8
                    CardTitle { appWindow: root.appWindow; text: appWindow.tr("battery_title"); onClicked: appWindow.navIndex = 5 }
                    RowLayout {
                        Layout.fillHeight: true
                        spacing: 12
                        Stat { appWindow: root.appWindow;
                            label: appWindow.tr("time_left")
                            value: !controller.connected ? "—" : controller.isCharging ? appWindow.tr("charging")
                                 : controller.batteryTimeLeft !== "" ? controller.batteryTimeLeft : "—"
                            note: controller.batterySessionStart > 0 && !controller.isCharging
                                ? appWindow.tr("battery_session_since").arg(Qt.formatTime(new Date(controller.batterySessionStart), "HH:mm")) : ""
                        }
                        Rectangle { width: 1; Layout.fillHeight: true; color: Theme.line }
                        Stat { appWindow: root.appWindow;
                            label: appWindow.tr("battery_rate")
                            value: controller.connected && controller.batteryDischargeRate > 0 ? controller.batteryDischargeRate.toFixed(1) + "%" : "—"
                            // A session with no rate yet is the normal state
                            // for the first half hour after a charge; say so
                            // rather than leave two dashes unexplained.
                            note: !controller.connected || controller.isCharging ? ""
                                : controller.batteryDischargeRate > 0 ? appWindow.tr("per_hour") : appWindow.tr("battery_estimate_pending")
                        }
                    }
                }
            }

            // Quick actions. Never narrower than its widest button: the
            // labels are the point of the card, so at the minimum window
            // width the two cards beside it give way (their values elide
            // and wrap) rather than "Открыть эквалайзер" losing its tail.
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 200
                Layout.minimumWidth: quickActions.implicitWidth + 36
                ColumnLayout {
                    id: quickActions
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10
                    CardTitle { appWindow: root.appWindow; text: appWindow.tr("quick_actions"); showChevron: false }
                    PillButton { appWindow: root.appWindow;
                        Layout.fillWidth: true
                        compact: true
                        text: appWindow.tr("open_equalizer")
                        glyphPath: appWindow.icons.sliders
                        onClicked: appWindow.navIndex = 2
                    }
                    PillButton { appWindow: root.appWindow;
                        Layout.fillWidth: true
                        compact: true
                        text: controller.connected ? appWindow.tr("power_off") : appWindow.tr("nav_device_switcher")
                        glyphPath: controller.connected ? appWindow.icons.power : appWindow.icons.swap
                        onClicked: { if (controller.connected) controller.powerOff(); else appWindow.navIndex = 4 }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    component CardTitle: RowLayout {
        id: cardTitle
        required property var appWindow
        property string text: ""
        property bool showChevron: true
        signal clicked()
        Layout.fillWidth: true
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: cardTitle.text
            color: Theme.txt
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
        Glyph { appWindow: cardTitle.appWindow; visible: cardTitle.showChevron; path: appWindow.icons.chevronRight; size: 16; color: Theme.txtDim }
        TapHandler { enabled: cardTitle.showChevron; onTapped: cardTitle.clicked() }
        HoverHandler { enabled: cardTitle.showChevron; cursorShape: Qt.PointingHandCursor }
    }

    component VitalRow: RowLayout {
        id: vital
        required property var appWindow
        property string glyph: ""
        property string title: ""
        property string detail: ""
        spacing: 12
        Rectangle {
            width: 34; height: 34; radius: 17
            color: Theme.surfaceHi
            border.width: 1
            border.color: Theme.line
            Glyph { appWindow: vital.appWindow; anchors.centerIn: parent; path: vital.glyph; size: 16; color: Theme.txt; weight: 1.7 }
        }
        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            Text { textFormat: Text.PlainText; text: vital.title; color: Theme.txt; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { textFormat: Text.PlainText; text: vital.detail; color: Theme.txtDim; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
        }
    }

    component Stat: ColumnLayout {
        id: stat
        required property var appWindow
        property string label: ""
        property string value: ""
        property string note: ""
        Layout.fillWidth: true
        // Equal columns: otherwise the row splits by label width and the
        // narrower stat is left with no room for its note.
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignTop
        spacing: 6
        // One line, elided: the card has no height for a wrapped label on
        // top of a two-line note, so the labels are kept short in every
        // language instead ("Разряд", "Décharge").
        Text { textFormat: Text.PlainText; text: stat.label; color: Theme.txtDim; font.pixelSize: 11
               elide: Text.ElideRight; Layout.fillWidth: true }
        DotText { text: stat.value; dot: 3.6; maxWidth: stat.width; color: Theme.txt }
        // Two lines: "Discharging since 13:30" does not fit one at the
        // minimum window width, and the card has the height to spare.
        Text { textFormat: Text.PlainText; visible: text !== ""; text: stat.note; color: Theme.txtDim; font.pixelSize: 11
               wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
