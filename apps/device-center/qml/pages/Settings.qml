import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../components"

ViewPage {
    id: root
    // More cards than fit the minimum window height, so this
    // page scrolls; the others still fit and don't.
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: settingsColumn.implicitHeight + 72
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: settingsColumn
        x: 32; y: 22
        width: parent.width - 64
        spacing: 16

        SectionTitle { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: false
            eyebrow: appWindow.tr("settings_eyebrow")
            title: appWindow.tr("settings_title")
            subtitle: appWindow.tr("settings_subtitle")
        }

        // Card 1: System & Interface Preferences
        Card {
            appWindow: root.appWindow
            Layout.fillWidth: true
            implicitHeight: appearanceColumn.implicitHeight + 40
            ColumnLayout {
                id: appearanceColumn
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14
                Eyebrow { appWindow: root.appWindow; text: appWindow.tr("appearance") }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: appWindow.tr("icon_antialiasing"); color: Theme.txt; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    NeoSwitch {
                        appWindow: root.appWindow
                        objectName: "iconSmoothingSwitch"
                        confirmedChecked: controller.iconAntialiasing
                        onToggled: controller.setIconAntialiasing(checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: appWindow.tr("theme"); color: Theme.txt; Layout.fillWidth: true }
                    RowLayout {
                        objectName: "themeSelector"
                        spacing: 6
                        Repeater {
                            model: [
                                { mode: "light",  label: appWindow.tr("theme_light") },
                                { mode: "dark",   label: appWindow.tr("theme_dark") },
                                { mode: "system", label: appWindow.tr("theme_system") }
                            ]
                            delegate: PillButton { appWindow: root.appWindow;
                                required property var modelData
                                compact: true
                                text: modelData.label
                                active: controller.themeMode === modelData.mode
                                onClicked: controller.setThemeMode(modelData.mode)
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: appWindow.tr("animations"); color: Theme.txt }
                        Text { text: appWindow.tr("animations_desc"); color: Theme.txtDim; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                    NeoSwitch {
                        appWindow: root.appWindow
                        objectName: "animationsSwitch"
                        confirmedChecked: controller.animationsEnabled
                        onToggled: controller.setAnimationsEnabled(checked)
                    }
                }
            }
        }

        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: systemColumn.implicitHeight + 44

            ColumnLayout {
                id: systemColumn
                anchors.fill: parent
                anchors.margins: 22
                spacing: 16

                // Row 1: Init with OS
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.controlRadius
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: appWindow.icons.power
                            size: 18
                            color: Theme.accentSoft
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("init_with_os")
                            color: Theme.txt
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("init_with_os_desc")
                            color: Theme.txtDim
                            font.pixelSize: 12
                        }
                    }

                    NeoSwitch { appWindow: root.appWindow;
                        confirmedChecked: controller.autostart
                        onToggled: controller.setAutostart(checked)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.line
                    visible: trayAvailable
                }

                // Row: minimise to tray (only offered when a tray exists)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    visible: trayAvailable

                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.controlRadius
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: appWindow.icons.headphones
                            size: 18
                            color: Theme.accentSoft
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("minimize_to_tray")
                            color: Theme.txt
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("minimize_to_tray_desc")
                            color: Theme.txtDim
                            font.pixelSize: 12
                        }
                    }

                    NeoSwitch { appWindow: root.appWindow;
                        confirmedChecked: controller.minimizeToTray
                        onToggled: controller.setMinimizeToTray(checked)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.line
                }

                // Row 2: Language Selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.controlRadius
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: appWindow.icons.globe
                            size: 18
                            color: Theme.accentSoft
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("language")
                            color: Theme.txt
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("language_desc")
                            color: Theme.txtDim
                            font.pixelSize: 12
                        }
                    }

                    ComboBox {
                        id: langCombo
                        implicitWidth: 168
                        implicitHeight: 38
                        model: controller.availableLanguages
                        textRole: "name"
                        valueRole: "code"

                        currentIndex: {
                            var langs = controller.availableLanguages
                            for (var i = 0; i < langs.length; ++i) {
                                if (langs[i].code === controller.currentLanguage) return i
                            }
                            return 0
                        }

                        onActivated: {
                            var item = model[index]
                            if (item && item.code) {
                                controller.setLanguage(item.code)
                            }
                        }

                        background: Rectangle {
                            radius: Theme.controlRadius
                            color: langCombo.hovered ? Theme.surfaceHi : Theme.surfaceSunk
                            border.width: 1
                            border.color: langCombo.hovered ? Theme.lineHi : Theme.line
                            Behavior on color { ColorAnimation { duration: Theme.tFast } }
                        }

                        contentItem: Text {
                            textFormat: Text.PlainText
                            leftPadding: 14
                            rightPadding: 28
                            text: langCombo.displayText
                            color: Theme.txt
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        indicator: Glyph { appWindow: root.appWindow;
                            x: langCombo.width - width - 12
                            y: langCombo.height / 2 - height / 2
                            size: 14
                            color: Theme.txtFaint
                            path: appWindow.icons.chevron
                            rotation: langCombo.popup.visible ? 180 : 0
                            Behavior on rotation { NumberAnimation { duration: Theme.tBase } }
                        }

                        popup: Popup {
                            y: langCombo.height + 4
                            width: langCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight + 12, 260)
                            padding: 6
                            background: Rectangle {
                                radius: Theme.controlRadius
                                color: Theme.surface
                                border.width: 1
                                border.color: Theme.lineHi
                            }
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: langCombo.popup.visible ? langCombo.delegateModel : null
                                currentIndex: langCombo.highlightedIndex
                                ScrollIndicator.vertical: ScrollIndicator {}
                            }
                        }

                        delegate: ItemDelegate {
                            id: langDel
                            width: langCombo.width - 12
                            implicitHeight: 36
                            highlighted: langCombo.highlightedIndex === index
                            hoverEnabled: true

                            background: Rectangle {
                                radius: 8
                                color: langDel.hovered ? Theme.surfaceHi : "transparent"
                            }

                            contentItem: RowLayout {
                                spacing: 8
                                Text {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: (modelData.code === controller.currentLanguage) ? Theme.accentSoft : Theme.txt
                                    font.pixelSize: 13
                                    font.weight: (modelData.code === controller.currentLanguage) ? Font.DemiBold : Font.Normal
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Rectangle {
                                    visible: modelData.code === controller.currentLanguage
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: Theme.accent
                                }
                            }
                        }
                    }
                }
            }
        }

        // Card 2: Notifications (they go through the tray icon)
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: notifyColumn.implicitHeight + 44
            visible: trayAvailable

            ColumnLayout {
                id: notifyColumn
                anchors.fill: parent
                anchors.margins: 22
                spacing: 16

                Eyebrow { appWindow: root.appWindow; text: appWindow.tr("notifications") }

                Repeater {
                    model: [
                        { key: "low", title: appWindow.tr("notify_low_battery_setting"), desc: appWindow.tr("notify_low_battery_setting_desc"), glyph: appWindow.icons.bolt },
                        { key: "conn", title: appWindow.tr("notify_connection_setting"), desc: appWindow.tr("notify_connection_setting_desc"), glyph: appWindow.icons.headphones },
                        { key: "charged", title: appWindow.tr("notify_charged_setting"), desc: appWindow.tr("notify_charged_setting_desc"), glyph: appWindow.icons.sparkle }
                    ]
                    delegate: RowLayout {
                        id: notifyRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: Theme.controlRadius
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                            Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: notifyRow.modelData.glyph; size: 18; color: Theme.accentSoft }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                textFormat: Text.PlainText
                                text: notifyRow.modelData.title
                                color: Theme.txt
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: notifyRow.modelData.desc
                                color: Theme.txtDim
                                font.pixelSize: 12
                            }
                        }

                        // Threshold picker sits only on the low-battery row.
                        ComboBox {
                            id: thresholdCombo
                            visible: notifyRow.modelData.key === "low"
                            implicitWidth: 92
                            implicitHeight: 38
                            model: [10, 15, 20, 25, 30]
                            currentIndex: Math.max(0, model.indexOf(controller.lowBatteryThreshold))
                            onActivated: controller.setLowBatteryThreshold(model[index])
                            displayText: currentText + "%"

                            background: Rectangle {
                                radius: Theme.controlRadius
                                color: thresholdCombo.hovered ? Theme.surfaceHi : Theme.surfaceSunk
                                border.width: 1
                                border.color: thresholdCombo.hovered ? Theme.lineHi : Theme.line
                            }
                            contentItem: Text {
                                textFormat: Text.PlainText
                                leftPadding: 13
                                rightPadding: 28
                                text: thresholdCombo.displayText
                                color: Theme.txt
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                            }
                            indicator: Glyph { appWindow: root.appWindow;
                                x: thresholdCombo.width - width - 12
                                y: thresholdCombo.height / 2 - height / 2
                                size: 14
                                color: Theme.txtFaint
                                path: appWindow.icons.chevron
                                rotation: thresholdCombo.popup.visible ? 180 : 0
                            }
                        }

                        NeoSwitch { appWindow: root.appWindow;
                            confirmedChecked: notifyRow.modelData.key === "low" ? controller.notifyLowBattery
                                            : notifyRow.modelData.key === "conn" ? controller.notifyConnection
                                            : controller.notifyCharged
                            onToggled: {
                                if (notifyRow.modelData.key === "low") controller.setNotifyLowBattery(checked)
                                else if (notifyRow.modelData.key === "conn") controller.setNotifyConnection(checked)
                                else controller.setNotifyCharged(checked)
                            }
                        }
                    }
                }
            }
        }

        // Split Cards: About Application & Community/Donate
        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            // Left Card: About App & Version
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.preferredHeight: 184

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 14

                    RowLayout {
                        spacing: 14
                        BrandTile { appWindow: root.appWindow;
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                textFormat: Text.PlainText
                                text: appWindow.tr("about_app")
                                color: Theme.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: "Sony Device Center"
                                color: Theme.txtDim
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: verLabel.implicitWidth + 20
                            radius: Theme.cardRadius
                            color: Theme.surfaceSunk
                            border.width: 1
                            border.color: Theme.lineHi

                            Text {
                                textFormat: Text.PlainText
                                id: verLabel
                                anchors.centerIn: parent
                                text: "v" + controller.appVersion
                                color: Theme.accentSoft
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.line
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        ColumnLayout {
                            spacing: 2
                            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("protocol_core") }
                            Text {
                                textFormat: Text.PlainText
                                text: "MDR V1 & V2 (C++20)"
                                color: Theme.txt
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("framework") }
                            Text {
                                textFormat: Text.PlainText
                                text: "Qt 6 Quick / QML"
                                color: Theme.txt
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("license") }
                            Text {
                                textFormat: Text.PlainText
                                text: appWindow.tr("license_value")
                                color: Theme.txt
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                        }
                    }
                }
            }

            // Right Card: GitHub & Donate
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.preferredHeight: 184

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 14

                    RowLayout {
                        spacing: 14
                        Rectangle {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                            radius: Theme.controlRadius
                            color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.16)
                            border.width: 1
                            border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.45)

                            Glyph { appWindow: root.appWindow;
                                anchors.centerIn: parent
                                path: appWindow.icons.heart
                                size: 20
                                color: Theme.danger
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                textFormat: Text.PlainText
                                text: appWindow.tr("links_support")
                                color: Theme.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: appWindow.tr("github_sponsorship")
                                color: Theme.txtDim
                                font.pixelSize: 12
                            }
                        }
                    }

                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: appWindow.tr("donate_desc")
                        color: Theme.txtDim
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        spacing: 10

                        PillButton { appWindow: root.appWindow;
                            compact: true
                            glyphPath: appWindow.icons.github
                            text: appWindow.tr("btn_github")
                            onClicked: controller.openUrl("https://github.com/marconvcm/sony-device-center")
                        }

                        PillButton { appWindow: root.appWindow;
                            compact: true
                            tint: Theme.danger
                            glyphPath: appWindow.icons.heart
                            text: appWindow.tr("btn_donate")
                            onClicked: controller.openUrl("https://github.com/sponsors/marconvcm")
                        }
                    }
                }
            }
        }

        // Independent-project notice. It lives on the page rather
        // than inside the About card because it is a trademark
        // statement, not a detail about the build.
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.preferredHeight: 1
            color: Theme.line
        }

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: appWindow.tr("disclaimer")
            color: Theme.txtFaint
            font.pixelSize: 12
            lineHeight: 1.4
            wrapMode: Text.WordWrap
        }

    }
    }
}
