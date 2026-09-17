pragma Singleton
import QtQuick

QtObject {
    // The sidebar is always dark, whichever mode the content area is in.
    readonly property color sidebarBg: "#141414"
    readonly property color sidebarSurface: "#212121"
    readonly property color sidebarSurfaceHi: "#2A2A2A"
    readonly property color sidebarSurfaceSunk: "#0F0F0F"
    readonly property color sidebarLine: "#2C2C2C"
    readonly property color sidebarLineHi: "#3D3D3D"
    readonly property color sidebarAccent: "#F2F2F2"
    readonly property color sidebarAccentSoft: "#FFFFFF"
    readonly property color sidebarAmbientWarm: "#F2F2F2"
    readonly property color sidebarSuccess: "#4ADE80"
    readonly property color sidebarDanger: "#F87171"
    readonly property color sidebarTxt: "#F2F2F2"
    readonly property color sidebarTxtDim: "#A3A3A3"
    readonly property color sidebarTxtFaint: "#6E6E6E"
    property bool iconAntialiasing: true
    property string mode: "dark"
    property bool animationsEnabled: true
    property bool systemReducedMotion: false
    readonly property bool light: mode === "light" || (mode === "system" && Qt.styleHints.colorScheme === Qt.Light)
    readonly property bool motionEnabled: animationsEnabled && !systemReducedMotion
    readonly property real cardRadius: 10
    readonly property real controlRadius: 8
    readonly property string bodyFamily: Qt.application.font.family
    function duration(milliseconds) { return motionEnabled ? milliseconds : 0 }

    // Monochrome palette: ink on paper by day, paper on ink by night.
    readonly property color bg:            light ? "#EDEDED" : "#0C0C0C"
    readonly property color surface:       light ? "#F8F8F8" : "#161616"
    readonly property color surfaceHi:     light ? "#FFFFFF" : "#1E1E1E"
    readonly property color surfaceSunk:   light ? "#E4E4E4" : "#101010"
    readonly property color line:          light ? "#DCDCDC" : "#272727"
    readonly property color lineHi:        light ? "#BEBEBE" : "#3B3B3B"

    readonly property color accent:        light ? "#111111" : "#F2F2F2"
    readonly property color accentSoft:    light ? "#3A3A3A" : "#CFCFCF"
    readonly property color accentText:      light ? "#FFFFFF" : "#111111"
    readonly property color ambientWarm:   accent
    readonly property color success:       light ? "#1F7A4D" : "#4ADE80"
    readonly property color danger:        light ? "#C0392B" : "#F87171"

    readonly property color txt:           light ? "#111111" : "#F2F2F2"
    readonly property color txtDim:        light ? "#5C5C5C" : "#9A9A9A"
    readonly property color txtFaint:      light ? "#8A8A8A" : "#6B6B6B"

    // Mono face for the wordmark. Resolved against the installed families
    // rather than set through font.families: that property makes the engine
    // abort loading the component on Qt 6.11, and silently — no error text.
    readonly property string monoFamily: {
        var wanted = ["JetBrains Mono", "JetBrainsMono Nerd Font", "Cascadia Code",
                      "SF Mono", "Consolas", "DejaVu Sans Mono", "Liberation Mono"]
        var installed = Qt.fontFamilies()
        for (var i = 0; i < wanted.length; ++i)
            if (installed.indexOf(wanted[i]) !== -1)
                return wanted[i]
        return "monospace"
    }

    // Motion constants — one place to retune the whole app's feel.
    readonly property int   tFast:  motionEnabled ? 140 : 0
    readonly property int   tBase:  motionEnabled ? 200 : 0
    readonly property int   tSlow:  motionEnabled ? 340 : 0

}
