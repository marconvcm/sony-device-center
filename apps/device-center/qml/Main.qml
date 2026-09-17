import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1180
    height: 780
    minimumWidth: 980
    minimumHeight: 660
    visible: !startHidden
    title: "Sony Device Center — " + controller.deviceName

    // Closing hides the window when a tray icon exists to bring it back;
    // quitting for real is the tray menu's job.
    onClosing: function(close) {
        if (trayAvailable && controller.minimizeToTray) {
            close.accepted = false
            window.hide()
        }
    }
    color: Theme.bg
    palette.window: Theme.bg
    palette.windowText: Theme.txt
    palette.base: Theme.surface
    palette.text: Theme.txt
    palette.button: Theme.surfaceHi
    palette.buttonText: Theme.txt
    palette.highlight: Theme.accent
    palette.highlightedText: "#FFFFFF"

    property int navIndex: 0

    // Reactive i18n helper
    function tr(key) {
        var _ = controller.currentLanguage
        return controller.t(key)
    }
    // Connection states come from the service as identifiers; translate the
    // known ones and show anything new verbatim rather than as a key name.
    function trState(state) {
        var key = "state_" + state
        var value = tr(key)
        return value === key ? state : value
    }
    function trPreset(id) {
        var key = "eq_preset_" + id
        var value = tr(key)
        return value === key ? controller.equalizerPresetName : value
    }

    Binding { target: Theme; property: "iconAntialiasing"; value: controller.iconAntialiasing }
    Binding { target: Theme; property: "mode"; value: controller.themeMode }
    Binding { target: Theme; property: "animationsEnabled"; value: controller.animationsEnabled }
    Binding { target: Theme; property: "systemReducedMotion"; value: controller.systemReducedMotion }

    // Icon library. Named, not scattered as magic strings.
    readonly property var icons: ({
        headphones: "M4 17v-4a8 8 0 0 1 16 0v4 M3.5 15h3.2v6H3.5z M17.3 15h3.2v6h-3.2z",
        shield:     "M12 3l7.5 3v6.2c0 4.6-3.2 7.6-7.5 9-4.3-1.4-7.5-4.4-7.5-9V6z",
        mic:        "M12 3.5a3 3 0 0 1 3 3v5.2a3 3 0 0 1-6 0V6.5a3 3 0 0 1 3-3z M5 11a7 7 0 0 0 14 0 M12 18.2V21",
        sliders:    "M6 21v-6.4 M6 11.2V3 M12 21v-9.6 M12 8V3 M18 21v-4 M18 13.6V3 M3.6 13h4.8 M9.6 10h4.8 M15.6 15h4.8",
        sparkle:    "M11 3l1.7 4.9L17.6 9.6 12.7 11.3 11 16.2 9.3 11.3 4.4 9.6 9.3 7.9z M18 15.4l.75 2.1 2.1.75-2.1.75-.75 2.1-.75-2.1-2.1-.75 2.1-.75z",
        swap:       "M4 8.5h13l-3.4-3.4 M20 15.5H7l3.4 3.4",
        power:      "M12 3.5v8 M6.6 6.6a7.6 7.6 0 1 0 10.8 0",
        bolt:       "M13.2 2.5L4.8 13.4h6.3l-1.3 8.1 8.4-10.9h-6.3z",
        battery:    "M3 7.5h13a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2z M21.5 10.5v3 M5 10.5v3 M8.5 10.5v3",
        bluetooth:  "M7.5 7.5L16.5 13.4 12 17V3.6l4.5 3.6-9 6",
        chevron:    "M5 9l7 7 7-7",
        chevronRight: "M9 5l7 7-7 7",
        home:       "M3 11l9-8 9 8 M5 9.5V21h14V9.5 M10 21v-6h4v6",
        waveform:   "M4 10v4 M8 7v10 M12 4v16 M16 7v10 M20 10v4",
        gridDots:   "M6 6h.01 M12 6h.01 M18 6h.01 M6 12h.01 M12 12h.01 M18 12h.01 M6 18h.01 M12 18h.01 M18 18h.01",
        ambient:    "M12.00 3.50m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M16.25 4.64m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M19.36 7.75m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M20.50 12.00m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M19.36 16.25m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M16.25 19.36m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M12.00 20.50m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M7.75 19.36m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M4.64 16.25m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M3.50 12.00m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M4.64 7.75m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0 M7.75 4.64m-.8 0a.8 .8 0 1 0 1.6 0a.8 .8 0 1 0-1.6 0",
        chat:       "M4 5h16v11H9l-5 4z",
        volume:     "M4 10v4h4l5 4V6L8 10z M16 9a4 4 0 0 1 0 6 M19 6a8 8 0 0 1 0 12",
        clock:      "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M12 8v5l3 2",
        batteryUp:  "M9 2.5h6 M7 5.5h10a1.5 1.5 0 0 1 1.5 1.5v13a1.5 1.5 0 0 1-1.5 1.5H7A1.5 1.5 0 0 1 5.5 20V7A1.5 1.5 0 0 1 7 5.5z M8.5 11.5h7v7h-7z",
        settings:   "M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z",
        globe:      "M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20z M2 12h20 M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z",
        github:     "M9 19c-5 1.5-5-2.5-7-3m14 6v-3.87a3.37 3.37 0 0 0-.94-2.61c3.14-.35 6.44-1.54 6.44-7A5.44 5.44 0 0 0 20 4.77 5.07 5.07 0 0 0 19.91 1S18.73.65 16 2.48a13.38 13.38 0 0 0-7 0C6.27.65 5.09 1 5.09 1A5.07 5.07 0 0 0 5 4.77a5.44 5.44 0 0 0-1.5 3.78c0 5.42 3.3 6.61 6.44 7A3.37 3.37 0 0 0 9 18.13V22",
        heart:      "M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 0 0-7.78 7.78l1.06 1.06L12 21.23l7.78-7.78 1.06-1.06a5.5 5.5 0 0 0 0-7.78z",
        info:       "M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20z M12 16v-4 M12 8h.01",
        refresh:    "M20 12a8 8 0 1 1-2.4-5.7 M20 4v5h-5",
        externalLink: "M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6 M15 3h6v6 M10 14L21 3"
    })

    // ==========================================================
    // SHELL
    // ==========================================================
    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar { appWindow: window }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            DeviceHeader { appWindow: window;
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.topMargin: 28
            }

            Rectangle { Layout.fillWidth: true; Layout.leftMargin: 32; Layout.rightMargin: 32; Layout.topMargin: 22; height: 1; color: Theme.line }

            // Not disabled while a command is in flight: disabling the tree drops
            // the mouse grab, which cut every slider drag short after its first
            // value. Repeated input coalesces in the controller instead.
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.navIndex

                Overview { appWindow: window }
                NoiseControl { appWindow: window }
                Equalizer { appWindow: window }
                Features { appWindow: window }
                DeviceSwitcher { appWindow: window }
                Battery { appWindow: window }
                Settings { appWindow: window }
            }

            // Status strip. Errors are the only thing allowed to shout here.
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.bottomMargin: 14
                Layout.topMargin: 6
                spacing: 12
                Text {
                    id: statusText
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: controller.lastError.length ? Theme.danger : Theme.txtFaint
                    font.pixelSize: 9
                    font.capitalization: Font.AllUppercase
                    text: controller.lastError.length ? controller.lastError :
                        controller.busy ? window.tr("working") : window.tr("connection_prefix") + window.trState(controller.connectionState)
                }
                Text {
                    textFormat: Text.PlainText
                    color: Theme.txtFaint
                    font.pixelSize: 9
                    font.capitalization: Font.AllUppercase
                    text: controller.deviceAddress.length ? controller.deviceAddress : "v" + controller.appVersion
                }
                Rectangle { width: 28; height: 1; color: Theme.txtFaint }
            }
        }
    }
}
