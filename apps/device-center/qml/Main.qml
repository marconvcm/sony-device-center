import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

ApplicationWindow {
    id: window
    width: 1180
    height: 780
    minimumWidth: 980
    minimumHeight: 660
    visible: true
    title: "Sony Device Center — " + controller.deviceName
    color: bg

    property int navIndex: 0

    // ==========================================================
    // DESIGN TOKENS
    // Every color in this file comes from here. No orphan hex.
    // ==========================================================
    readonly property color bg:            "#0A0B0F"
    readonly property color surface:       "#14161E"
    readonly property color surfaceHi:     "#1B1E29"
    readonly property color surfaceSunk:   "#0E1016"
    readonly property color line:          "#22252F"
    readonly property color lineHi:        "#2F3341"

    readonly property color accent:        "#7C5CFF"
    readonly property color accentSoft:    "#A78BFA"
    readonly property color ambientWarm:   "#F2A73B"
    readonly property color success:       "#2DD4A7"
    readonly property color danger:        "#FF5A5F"

    readonly property color txt:           "#F4F6FA"
    readonly property color txtDim:        "#98A1B2"
    readonly property color txtFaint:      "#5C6473"

    // Motion constants — one place to retune the whole app's feel.
    readonly property int   tFast:  140
    readonly property int   tBase:  200
    readonly property int   tSlow:  340

    // ==========================================================
    // REUSABLE PIECES
    // ==========================================================

    // Stroked SVG glyph. One string may hold several subpaths.
    component Glyph: Item {
        id: glyph
        property string path: ""
        property real size: 18
        property color color: window.txtDim
        property real weight: 1.8
        implicitWidth: size
        implicitHeight: size

        Shape {
            anchors.fill: parent
            antialiasing: true
            ShapePath {
                strokeColor: glyph.color
                // Path.scale scales geometry only — strokeWidth is already in
                // item pixels, so pre-multiplying it just makes icons look faint.
                strokeWidth: glyph.weight
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                scale: Qt.size(glyph.size / 24, glyph.size / 24)
                PathSvg { path: glyph.path }
            }
        }
        Behavior on color { ColorAnimation { duration: window.tFast } }
    }

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
        bluetooth:  "M7.5 7.5L16.5 13.4 12 17V3.6l4.5 3.6-9 6",
        chevron:    "M5 9l7 7 7-7"
    })

    // Elevated card. Gradient fakes a top light source; hairline defines the edge.
    component Card: Rectangle {
        id: card
        property bool active: false
        property bool hovered: false
        radius: 18
        border.width: 1
        border.color: active ? Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.55)
                    : hovered ? window.lineHi : window.line
        gradient: Gradient {
            GradientStop { position: 0.0; color: card.hovered ? window.surfaceHi : window.surface }
            GradientStop { position: 1.0; color: window.surfaceSunk }
        }
        Behavior on border.color { ColorAnimation { duration: window.tBase } }
    }

    // Soft radial bloom. Atmosphere only — never carries information.
    component Bloom: Shape {
        id: bloom
        property color tint: window.accent
        property real strength: 0.22
        antialiasing: true
        ShapePath {
            strokeWidth: -1
            fillGradient: RadialGradient {
                centerX: bloom.width / 2
                centerY: bloom.height / 2
                centerRadius: bloom.width / 2
                focalX: centerX
                focalY: centerY
                GradientStop { position: 0.0; color: Qt.rgba(bloom.tint.r, bloom.tint.g, bloom.tint.b, bloom.strength) }
                GradientStop { position: 0.55; color: Qt.rgba(bloom.tint.r, bloom.tint.g, bloom.tint.b, bloom.strength * 0.35) }
                GradientStop { position: 1.0; color: Qt.rgba(bloom.tint.r, bloom.tint.g, bloom.tint.b, 0.0) }
            }
            startX: 0; startY: 0
            PathLine { x: bloom.width; y: 0 }
            PathLine { x: bloom.width; y: bloom.height }
            PathLine { x: 0; y: bloom.height }
        }
    }

    // Small uppercase label. Letter-spacing is what makes it read as deliberate.
    component Eyebrow: Text {
        color: window.txtFaint
        font.pixelSize: 10
        font.bold: true
        font.letterSpacing: 1.4
        font.capitalization: Font.AllUppercase
    }

    // The pill button. Press physics live here so every button feels identical.
    component PillButton: Button {
        id: pill
        property color tint: window.accent
        property bool active: false
        property string glyphPath: ""
        property bool compact: false

        implicitHeight: compact ? 40 : 48
        // Padding is declared, not inherited, so implicitWidth can be derived
        // from it. Otherwise the control sizes contentItem to availableWidth,
        // the row is wider than it needs, and the slack lands on the right.
        topPadding: 0
        bottomPadding: 0
        leftPadding: compact ? 16 : 20
        rightPadding: leftPadding
        implicitWidth: pillRow.implicitWidth + leftPadding + rightPadding
        hoverEnabled: true
        scale: pressed ? 0.955 : (hovered ? 1.035 : 1.0)

        Behavior on scale {
            NumberAnimation { duration: 220; easing.type: Easing.OutBack; easing.overshoot: 2.2 }
        }

        background: Rectangle {
            radius: height / 2
            color: pill.active ? Qt.rgba(pill.tint.r, pill.tint.g, pill.tint.b, 0.16)
                 : pill.hovered ? window.surfaceHi : window.surface
            border.width: 1
            border.color: pill.active ? Qt.rgba(pill.tint.r, pill.tint.g, pill.tint.b, 0.8)
                        : pill.hovered ? window.lineHi : window.line

            Behavior on color { ColorAnimation { duration: window.tBase } }
            Behavior on border.color { ColorAnimation { duration: window.tBase } }

            // Halo ring — the difference between "on" and "on, and you felt it".
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: height / 2
                color: "transparent"
                border.width: 1
                border.color: pill.tint
                opacity: pill.active ? 0.4 : 0
                scale: pill.active ? 1.0 : 0.94
                Behavior on opacity { NumberAnimation { duration: window.tSlow } }
                Behavior on scale { NumberAnimation { duration: window.tSlow; easing.type: Easing.OutBack } }
            }
        }

        contentItem: RowLayout {
            id: pillRow
            // The glyph box is 24 units wide but most paths don't fill it, so
            // the optical gap is always a few px wider than this number.
            spacing: pill.compact ? 7 : 8
            Glyph {
                visible: pill.glyphPath !== ""
                path: pill.glyphPath
                size: pill.compact ? 17 : 20
                weight: 1.9
                color: pill.active ? pill.tint : window.txtDim
            }
            Text {
                text: pill.text
                color: pill.active ? window.txt : window.txtDim
                font.pixelSize: pill.compact ? 12 : 13
                font.weight: pill.active ? Font.DemiBold : Font.Medium
                Behavior on color { ColorAnimation { duration: window.tFast } }
            }
        }
    }

    // Custom switch. The stock one belongs to a different app.
    component NeoSwitch: Switch {
        id: sw
        implicitWidth: 50
        implicitHeight: 28
        hoverEnabled: true

        indicator: Rectangle {
            implicitWidth: 50
            implicitHeight: 28
            radius: height / 2
            color: sw.checked ? window.accent : window.surfaceSunk
            border.width: 1
            border.color: sw.checked ? window.accent : (sw.hovered ? window.lineHi : window.line)
            Behavior on color { ColorAnimation { duration: window.tBase } }
            Behavior on border.color { ColorAnimation { duration: window.tBase } }

            Rectangle {
                width: 20
                height: 20
                radius: 10
                y: 4
                x: sw.checked ? parent.width - width - 4 : 4
                color: sw.checked ? "white" : window.txtFaint
                Behavior on x { NumberAnimation { duration: 240; easing.type: Easing.OutBack; easing.overshoot: 1.4 } }
                Behavior on color { ColorAnimation { duration: window.tBase } }
            }
        }
        contentItem: Item {}
    }

    // Horizontal slider with a gradient fill and a handle that reacts.
    component NeoSlider: Slider {
        id: sl
        implicitHeight: 26
        hoverEnabled: true

        background: Rectangle {
            x: sl.leftPadding
            y: sl.topPadding + sl.availableHeight / 2 - height / 2
            width: sl.availableWidth
            height: 6
            radius: 3
            color: window.surfaceSunk
            border.width: 1
            border.color: window.line

            Rectangle {
                width: sl.visualPosition * parent.width
                height: parent.height
                radius: 3
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: window.accent }
                    GradientStop { position: 1.0; color: window.accentSoft }
                }
            }
        }

        handle: Rectangle {
            x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
            y: sl.topPadding + sl.availableHeight / 2 - height / 2
            width: 18
            height: 18
            radius: 9
            color: "white"
            border.width: 2
            border.color: window.accent
            scale: sl.pressed ? 1.25 : (sl.hovered ? 1.12 : 1.0)
            Behavior on scale {
                NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 2.5 }
            }
        }
    }

    // Vertical EQ band. Fills outward from the zero line, because that's what it means.
    component BandSlider: ColumnLayout {
        id: band
        property string label: ""
        property real value: 0
        signal moved(real v)

        spacing: 10

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: (band.value > 0 ? "+" : "") + Math.round(band.value)
            color: Math.round(band.value) === 0 ? window.txtFaint : window.accentSoft
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Behavior on color { ColorAnimation { duration: window.tFast } }
        }

        Slider {
            id: vs
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            orientation: Qt.Vertical
            from: -10
            to: 10
            stepSize: 1
            value: band.value
            implicitWidth: 34
            hoverEnabled: true
            onMoved: band.moved(value)

            background: Rectangle {
                x: vs.leftPadding + vs.availableWidth / 2 - width / 2
                y: vs.topPadding
                width: 6
                height: vs.availableHeight
                radius: 3
                color: window.surfaceSunk
                border.width: 1
                border.color: window.line

                // The zero line. Small detail, big legibility win.
                Rectangle {
                    width: 14
                    height: 1
                    x: -4
                    y: parent.height / 2
                    color: window.lineHi
                }

                Rectangle {
                    readonly property real p: 1 - vs.visualPosition
                    width: parent.width
                    y: parent.height * (1 - Math.max(0.5, p))
                    height: parent.height * Math.abs(p - 0.5)
                    radius: 3
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: window.accentSoft }
                        GradientStop { position: 1.0; color: window.accent }
                    }
                }
            }

            handle: Rectangle {
                x: vs.leftPadding + vs.availableWidth / 2 - width / 2
                y: vs.topPadding + vs.visualPosition * (vs.availableHeight - height)
                width: 20
                height: 20
                radius: 10
                color: "white"
                border.width: 2
                border.color: window.accent
                scale: vs.pressed ? 1.22 : (vs.hovered ? 1.1 : 1.0)
                Behavior on scale {
                    NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 2.5 }
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: band.label
            color: window.txtFaint
            font.pixelSize: 10
            font.letterSpacing: 0.6
        }
    }

    // Every view enters the same way. Consistency is the whole point.
    component ViewPage: Item {
        id: page
        default property alias pageData: inner.data

        Item {
            id: inner
            width: parent.width
            height: parent.height
            opacity: 0
        }

        ParallelAnimation {
            id: entrance
            NumberAnimation { target: inner; property: "opacity"; from: 0; to: 1; duration: window.tBase + 60; easing.type: Easing.OutQuad }
            NumberAnimation { target: inner; property: "y"; from: 16; to: 0; duration: window.tSlow; easing.type: Easing.OutCubic }
        }

        onVisibleChanged: if (visible) entrance.restart()
        Component.onCompleted: if (visible) entrance.restart()
    }

    // ==========================================================
    // ATMOSPHERE
    // ==========================================================
    Bloom {
        width: 760; height: 760
        x: 140; y: -360
        tint: window.accent
        strength: 0.13
    }
    Bloom {
        width: 640; height: 640
        x: window.width - 380
        y: window.height - 340
        tint: "#3B82F6"
        strength: 0.10
    }

    // ==========================================================
    // SHELL
    // ==========================================================
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------------
        // SIDEBAR
        // ------------------------------------------------------
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 258
            color: Qt.rgba(0.05, 0.055, 0.075, 0.72)

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: window.line
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 26

                // Brand
                RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 13
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: window.accentSoft }
                            GradientStop { position: 1.0; color: window.accent }
                        }
                        Glyph {
                            anchors.centerIn: parent
                            path: window.icons.headphones
                            size: 22
                            color: "white"
                            weight: 2
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        Text {
                            text: "Device Center"
                            color: window.txt
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.2
                        }
                        Eyebrow { text: "Sony Audio" }
                    }
                }

                // Device badge with a live battery ring
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    active: controller.connected

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 13

                        Item {
                            Layout.preferredWidth: 46
                            Layout.preferredHeight: 46

                            Canvas {
                                id: ring
                                anchors.fill: parent
                                property real level: controller.connected ? controller.batteryLevel : 0
                                Behavior on level { NumberAnimation { duration: 700; easing.type: Easing.OutCubic } }
                                onLevelChanged: requestPaint()

                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    var cx = width / 2, cy = height / 2, r = width / 2 - 4
                                    ctx.lineWidth = 3.5
                                    ctx.lineCap = "round"

                                    ctx.strokeStyle = "#22252F"
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r, 0, Math.PI * 2)
                                    ctx.stroke()

                                    if (level > 0) {
                                        ctx.strokeStyle = level > 20 ? "#2DD4A7" : "#FF5A5F"
                                        ctx.beginPath()
                                        ctx.arc(cx, cy, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * (level / 100))
                                        ctx.stroke()
                                    }
                                }
                            }

                            Glyph {
                                anchors.centerIn: parent
                                visible: controller.isCharging
                                path: window.icons.bolt
                                size: 16
                                color: window.success
                                weight: 2

                                SequentialAnimation on opacity {
                                    running: controller.isCharging
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.35; duration: 900; easing.type: Easing.InOutQuad }
                                    NumberAnimation { to: 1.0; duration: 900; easing.type: Easing.InOutQuad }
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !controller.isCharging
                                text: controller.connected ? controller.batteryLevel : "—"
                                color: window.txt
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                Layout.fillWidth: true
                                text: controller.deviceName
                                color: window.txt
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    Layout.preferredWidth: 6
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: controller.connected ? window.success : window.danger

                                    SequentialAnimation on opacity {
                                        running: controller.connected
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 0.3; duration: 1100; easing.type: Easing.InOutQuad }
                                        NumberAnimation { to: 1.0; duration: 1100; easing.type: Easing.InOutQuad }
                                    }
                                }
                                Text {
                                    text: controller.connected ? "Connected" : "Disconnected"
                                    color: controller.connected ? window.txtDim : window.txtFaint
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }

                // Navigation with a sliding indicator
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 5 * 44 + 4 * 6

                    // The indicator floats; items don't each carry their own.
                    Rectangle {
                        width: parent.width
                        height: 44
                        radius: 12
                        y: window.navIndex * 50
                        color: Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.45)

                        Behavior on y {
                            NumberAnimation { duration: 320; easing.type: Easing.OutBack; easing.overshoot: 1.1 }
                        }

                        Rectangle {
                            width: 3
                            height: 18
                            radius: 2
                            x: -1
                            anchors.verticalCenter: parent.verticalCenter
                            color: window.accent
                        }
                    }

                    Repeater {
                        model: [
                            { idx: 0, name: "Overview",        glyph: window.icons.headphones },
                            { idx: 1, name: "Noise Control",   glyph: window.icons.shield },
                            { idx: 2, name: "Equalizer",       glyph: window.icons.sliders },
                            { idx: 3, name: "Audio Features",  glyph: window.icons.sparkle },
                            { idx: 4, name: "Device Switcher", glyph: window.icons.swap }
                        ]

                        delegate: Item {
                            id: navItem
                            required property var modelData
                            readonly property bool current: window.navIndex === modelData.idx

                            width: parent.width
                            height: 44
                            y: modelData.idx * 50

                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: (navHover.hovered && !navItem.current) ? window.surfaceHi : "transparent"
                                Behavior on color { ColorAnimation { duration: window.tFast } }
                            }

                            HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 13

                                Glyph {
                                    path: navItem.modelData.glyph
                                    size: 19
                                    color: navItem.current ? window.accentSoft
                                         : navHover.hovered ? window.txt : window.txtFaint
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: navItem.modelData.name
                                    color: navItem.current ? window.txt
                                         : navHover.hovered ? window.txtDim : window.txtFaint
                                    font.pixelSize: 13
                                    font.weight: navItem.current ? Font.DemiBold : Font.Normal
                                    Behavior on color { ColorAnimation { duration: window.tFast } }
                                }
                            }

                            TapHandler { onTapped: window.navIndex = navItem.modelData.idx }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Transport / daemon status
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 12
                    color: window.surfaceSunk
                    border.width: 1
                    border.color: window.line

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 13
                        anchors.rightMargin: 13
                        spacing: 9

                        Glyph { path: window.icons.bluetooth; size: 15; color: window.success }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                text: "SDK Core"
                                color: window.txtDim
                                font.pixelSize: 11
                                font.weight: Font.Medium
                            }
                            Text {
                                text: "IPC · RFCOMM V2"
                                color: window.txtFaint
                                font.pixelSize: 10
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 3.5
                            color: window.success
                        }
                    }
                }
            }
        }

        // ------------------------------------------------------
        // CONTENT
        // ------------------------------------------------------
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.navIndex

            // ==================================================
            // 1 · OVERVIEW
            // ==================================================
            ViewPage {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 36
                    spacing: 22

                    // Header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ColumnLayout {
                            spacing: 5
                            Eyebrow { text: controller.connected ? "Connected Device" : "Offline" }
                            Text {
                                text: controller.deviceName
                                color: window.txt
                                font.pixelSize: 30
                                font.weight: Font.DemiBold
                                font.letterSpacing: -0.7
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Compact status chips
                        Repeater {
                            model: [
                                { k: "Codec", v: "LDAC" },
                                { k: "Battery", v: controller.batteryLevel + "%" },
                                { k: "Mode", v: controller.noiseControlMode === "cancelling" ? "ANC"
                                              : controller.noiseControlMode === "ambient" ? "Ambient" : "Off" }
                            ]

                            delegate: Rectangle {
                                id: statChip
                                required property var modelData
                                implicitWidth: chipCol.implicitWidth + 30
                                implicitHeight: 54
                                radius: 14
                                color: window.surface
                                border.width: 1
                                border.color: window.line

                                ColumnLayout {
                                    id: chipCol
                                    anchors.centerIn: parent
                                    spacing: 3
                                    Eyebrow {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: statChip.modelData.k
                                    }
                                    Text {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: statChip.modelData.v
                                        color: window.txt
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                        }
                    }

                    // Hero
                    Card {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 24
                        clip: true

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 4

                            // The aura belongs to the product, not the card.
                            // Centering it here keeps the rings off the labels.
                            Item {
                                id: heroStage
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 290
                                Layout.preferredHeight: 290

                                Bloom {
                                    anchors.centerIn: parent
                                    z: -1
                                    width: 470; height: 470
                                    tint: controller.noiseControlMode === "cancelling" ? window.accent
                                        : controller.noiseControlMode === "ambient" ? window.ambientWarm
                                        : window.txtFaint
                                    strength: controller.noiseControlMode === "off" ? 0.05 : 0.18
                                    Behavior on strength { NumberAnimation { duration: window.tSlow } }
                                }

                                // Concentric rings. ANC pulls inward, Ambient opens outward.
                                Repeater {
                                    model: 3
                                    delegate: Rectangle {
                                        id: auraRing
                                        required property int index
                                        readonly property bool inward: controller.noiseControlMode === "cancelling"
                                        readonly property bool live: controller.noiseControlMode !== "off"

                                        anchors.centerIn: parent
                                        z: -1
                                        width: 252 + index * 58
                                        height: width
                                        radius: width / 2
                                        color: "transparent"
                                        border.width: 1
                                        border.color: inward ? window.accent : window.ambientWarm
                                        opacity: 0
                                        visible: live

                                        SequentialAnimation {
                                            running: auraRing.live
                                            loops: Animation.Infinite
                                            PauseAnimation { duration: auraRing.index * 700 }
                                            ParallelAnimation {
                                                NumberAnimation {
                                                    target: auraRing; property: "opacity"
                                                    from: 0.0; to: 0.28
                                                    duration: 900; easing.type: Easing.OutQuad
                                                }
                                                NumberAnimation {
                                                    target: auraRing; property: "scale"
                                                    from: auraRing.inward ? 1.12 : 0.90
                                                    to: 1.0
                                                    duration: 900; easing.type: Easing.OutQuad
                                                }
                                            }
                                            ParallelAnimation {
                                                NumberAnimation {
                                                    target: auraRing; property: "opacity"
                                                    to: 0.0
                                                    duration: 1200; easing.type: Easing.InQuad
                                                }
                                                NumberAnimation {
                                                    target: auraRing; property: "scale"
                                                    to: auraRing.inward ? 0.88 : 1.14
                                                    duration: 1200; easing.type: Easing.InQuad
                                                }
                                            }
                                            PauseAnimation { duration: 400 }
                                        }
                                    }
                                }

                                Image {
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectFit
                                    source: "../" + controller.heroImagePath
                                    opacity: controller.connected ? 1.0 : 0.35
                                    scale: heroHover.hovered ? 1.06 : 1.0

                                    Behavior on scale { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
                                    Behavior on opacity { NumberAnimation { duration: window.tSlow } }

                                    HoverHandler { id: heroHover }
                                }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 10
                                text: controller.noiseControlMode === "cancelling" ? "Noise Cancelling"
                                    : controller.noiseControlMode === "ambient" ? "Ambient Sound"
                                    : "Processing Off"
                                color: window.txt
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                font.letterSpacing: -0.2
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 2
                                text: controller.noiseControlMode === "cancelling" ? "The outside world is sealed out"
                                    : controller.noiseControlMode === "ambient" ? "Level " + controller.ambientLevel + " · hearing your surroundings"
                                    : "Straight signal, no processing"
                                color: window.txtFaint
                                font.pixelSize: 12
                            }

                            // Quick actions
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 30
                                spacing: 11

                                PillButton {
                                    text: "Noise Cancelling"
                                    glyphPath: window.icons.shield
                                    tint: window.accent
                                    active: controller.noiseControlMode === "cancelling"
                                    onClicked: controller.setAnc(!active)
                                }

                                PillButton {
                                    text: "Ambient"
                                    glyphPath: window.icons.mic
                                    tint: window.ambientWarm
                                    active: controller.noiseControlMode === "ambient"
                                    onClicked: controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
                                }

                                PillButton {
                                    text: "Off"
                                    glyphPath: window.icons.power
                                    tint: window.txtDim
                                    active: controller.noiseControlMode === "off"
                                    onClicked: controller.setNoiseControlOff()
                                }
                            }

                            // Secondary: a link to another screen, not a fourth mode.
                            PillButton {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 12
                                compact: true
                                text: "EQ · " + controller.equalizerPresetName
                                glyphPath: window.icons.sliders
                                onClicked: window.navIndex = 2
                            }
                        }
                    }
                }
            }

            // ==================================================
            // 2 · NOISE CONTROL
            // ==================================================
            ViewPage {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 36
                    spacing: 24

                    ColumnLayout {
                        spacing: 5
                        Eyebrow { text: "Isolation" }
                        Text {
                            text: "Noise Control"
                            color: window.txt
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.6
                        }
                        Text {
                            text: "Choose how much of the world gets through."
                            color: window.txtDim
                            font.pixelSize: 13
                        }
                    }

                    // Mode cards — big targets, honest states
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 132
                        spacing: 14

                        Repeater {
                            model: [
                                { mode: "cancelling", label: "Noise Cancelling", glyph: window.icons.shield, desc: "Seal out the room", tint: window.accent },
                                { mode: "ambient",    label: "Ambient Sound",    glyph: window.icons.mic,    desc: "Let the room in",   tint: window.ambientWarm },
                                { mode: "off",        label: "Off",              glyph: window.icons.power,  desc: "No processing",     tint: window.txtDim }
                            ]

                            delegate: Card {
                                id: modeCard
                                required property var modelData
                                readonly property bool current: controller.noiseControlMode === modelData.mode

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                active: current
                                hovered: modeHover.hovered
                                scale: modeTap.pressed ? 0.975 : (modeHover.hovered ? 1.012 : 1.0)

                                Behavior on scale {
                                    NumberAnimation { duration: 220; easing.type: Easing.OutBack; easing.overshoot: 1.8 }
                                }

                                HoverHandler { id: modeHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler {
                                    id: modeTap
                                    onTapped: {
                                        if (modeCard.modelData.mode === "cancelling") controller.setAnc(true)
                                        else if (modeCard.modelData.mode === "ambient") controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
                                        else controller.setNoiseControlOff()
                                    }
                                }

                                ColumnLayout {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 20
                                    anchors.rightMargin: 20
                                    spacing: 12

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Rectangle {
                                            Layout.preferredWidth: 42
                                            Layout.preferredHeight: 42
                                            radius: 13
                                            color: modeCard.current
                                                 ? Qt.rgba(modeCard.modelData.tint.r, modeCard.modelData.tint.g, modeCard.modelData.tint.b, 0.18)
                                                 : window.surfaceSunk
                                            border.width: 1
                                            border.color: modeCard.current
                                                 ? Qt.rgba(modeCard.modelData.tint.r, modeCard.modelData.tint.g, modeCard.modelData.tint.b, 0.5)
                                                 : window.line
                                            Behavior on color { ColorAnimation { duration: window.tBase } }
                                            Behavior on border.color { ColorAnimation { duration: window.tBase } }

                                            Glyph {
                                                anchors.centerIn: parent
                                                path: modeCard.modelData.glyph
                                                size: 21
                                                color: modeCard.current ? modeCard.modelData.tint : window.txtFaint
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Active dot, animated in
                                        Rectangle {
                                            Layout.preferredWidth: 9
                                            Layout.preferredHeight: 9
                                            radius: 4.5
                                            color: modeCard.modelData.tint
                                            opacity: modeCard.current ? 1 : 0
                                            scale: modeCard.current ? 1 : 0.4
                                            Behavior on opacity { NumberAnimation { duration: window.tBase } }
                                            Behavior on scale { NumberAnimation { duration: 280; easing.type: Easing.OutBack; easing.overshoot: 3 } }
                                        }
                                    }

                                    ColumnLayout {
                                        spacing: 2
                                        Text {
                                            text: modeCard.modelData.label
                                            color: window.txt
                                            font.pixelSize: 15
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            text: modeCard.modelData.desc
                                            color: window.txtFaint
                                            font.pixelSize: 12
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Ambient detail
                    Card {
                        id: ambientCard
                        Layout.fillWidth: true
                        Layout.preferredHeight: 178
                        readonly property bool live: controller.noiseControlMode === "ambient"
                        opacity: live ? 1.0 : 0.42
                        enabled: live
                        Behavior on opacity { NumberAnimation { duration: window.tSlow; easing.type: Easing.OutQuad } }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 22
                            spacing: 18

                            RowLayout {
                                Layout.fillWidth: true
                                ColumnLayout {
                                    spacing: 2
                                    Eyebrow { text: "Ambient" }
                                    Text {
                                        text: "Sound Level"
                                        color: window.txt
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    implicitWidth: 56
                                    implicitHeight: 34
                                    radius: 11
                                    color: Qt.rgba(window.ambientWarm.r, window.ambientWarm.g, window.ambientWarm.b, 0.16)
                                    border.width: 1
                                    border.color: Qt.rgba(window.ambientWarm.r, window.ambientWarm.g, window.ambientWarm.b, 0.5)
                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(ambientSlider.value)
                                        color: window.ambientWarm
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            NeoSlider {
                                id: ambientSlider
                                Layout.fillWidth: true
                                from: 1; to: 20; stepSize: 1
                                value: controller.ambientLevel
                                onMoved: controller.setAmbient(Math.round(value), voiceSwitch.checked)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: "Focus on Voice"
                                        color: window.txt
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                    }
                                    Text {
                                        text: "Lift human speech, filter the low rumble"
                                        color: window.txtFaint
                                        font.pixelSize: 11
                                    }
                                }
                                NeoSwitch {
                                    id: voiceSwitch
                                    checked: controller.focusOnVoice
                                    onToggled: controller.setAmbient(controller.ambientLevel, checked)
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ==================================================
            // 3 · EQUALIZER
            // ==================================================
            ViewPage {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 36
                    spacing: 20

                    ColumnLayout {
                        spacing: 5
                        Eyebrow { text: "Signature" }
                        Text {
                            text: "Equalizer"
                            color: window.txt
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.6
                        }
                        Text {
                            text: "Five bands, plus dedicated Clear Bass."
                            color: window.txtDim
                            font.pixelSize: 13
                        }
                    }

                    // Presets — wrapping flow, not ten crushed columns
                    Flow {
                        Layout.fillWidth: true
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
                                id: chip
                                required property var modelData
                                readonly property bool current: controller.equalizerPreset === modelData.id

                                width: chipText.implicitWidth + 30
                                height: 38
                                radius: 19
                                color: current ? Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.18)
                                     : chipHover.hovered ? window.surfaceHi : window.surface
                                border.width: 1
                                border.color: current ? Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.7)
                                            : chipHover.hovered ? window.lineHi : window.line

                                Behavior on color { ColorAnimation { duration: window.tFast } }
                                Behavior on border.color { ColorAnimation { duration: window.tFast } }

                                scale: chipTap.pressed ? 0.94 : (chipHover.hovered ? 1.04 : 1.0)
                                Behavior on scale {
                                    NumberAnimation { duration: 200; easing.type: Easing.OutBack; easing.overshoot: 2.4 }
                                }

                                HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { id: chipTap; onTapped: controller.setEqualizerPreset(chip.modelData.id) }

                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: chip.modelData.name
                                    color: chip.current ? window.txt : window.txtDim
                                    font.pixelSize: 12
                                    font.weight: chip.current ? Font.DemiBold : Font.Normal
                                    Behavior on color { ColorAnimation { duration: window.tFast } }
                                }
                            }
                        }
                    }

                    // The five bands — the reason anyone opens this screen
                    Card {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 14

                            RowLayout {
                                Layout.fillWidth: true
                                Eyebrow { text: "5-Band · ±10 dB" }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: controller.equalizerPresetName
                                    color: window.accentSoft
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 4

                                Repeater {
                                    model: ["400", "1k", "2.5k", "6.3k", "16k"]

                                    delegate: BandSlider {
                                        id: bandItem
                                        required property int index
                                        required property var modelData

                                        Layout.fillWidth: true
                                        Layout.fillHeight: true

                                        label: modelData
                                        value: (controller.equalizerBands && controller.equalizerBands[index] !== undefined)
                                               ? controller.equalizerBands[index] : 0

                                        onMoved: function(v) {
                                            var next = []
                                            for (var i = 0; i < 5; ++i) {
                                                next.push(i === bandItem.index
                                                    ? Math.round(v)
                                                    : ((controller.equalizerBands && controller.equalizerBands[i] !== undefined)
                                                        ? controller.equalizerBands[i] : 0))
                                            }
                                            controller.setEqualizerCustom(controller.clearBass, next)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Clear Bass
                    Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 96

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 22
                            anchors.rightMargin: 22
                            spacing: 22

                            ColumnLayout {
                                Layout.preferredWidth: 210
                                spacing: 2
                                Text {
                                    text: "Clear Bass"
                                    color: window.txt
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: "Sub-bass weight, no distortion"
                                    color: window.txtFaint
                                    font.pixelSize: 11
                                }
                            }

                            NeoSlider {
                                Layout.fillWidth: true
                                from: -10; to: 10; stepSize: 1
                                value: controller.clearBass
                                onMoved: controller.setEqualizerCustom(Math.round(value), controller.equalizerBands)
                            }

                            Rectangle {
                                implicitWidth: 56
                                implicitHeight: 34
                                radius: 11
                                color: Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.16)
                                border.width: 1
                                border.color: Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.5)
                                Text {
                                    anchors.centerIn: parent
                                    text: (controller.clearBass > 0 ? "+" : "") + controller.clearBass
                                    color: window.accentSoft
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }
            }

            // ==================================================
            // 4 · AUDIO FEATURES
            // ==================================================
            ViewPage {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 36
                    spacing: 22

                    ColumnLayout {
                        spacing: 5
                        Eyebrow { text: "Behaviour" }
                        Text {
                            text: "Sound & Device Features"
                            color: window.txt
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.6
                        }
                        Text {
                            text: "Upscaling, speak-to-chat, and power discipline."
                            color: window.txtDim
                            font.pixelSize: 13
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 14
                        columnSpacing: 14

                        Repeater {
                            model: [
                                { key: "dsee",     title: "DSEE Extreme",    desc: "AI restores the highs that compression threw away", glyph: window.icons.sparkle },
                                { key: "speak",    title: "Speak-to-Chat",   desc: "Pauses playback and opens ambient when you talk",   glyph: window.icons.mic },
                                { key: "adaptive", title: "Adaptive Volume", desc: "Balances level against your surroundings",          glyph: window.icons.sliders }
                            ]

                            delegate: Card {
                                id: featCard
                                required property var modelData
                                readonly property bool on: modelData.key === "dsee" ? controller.dsee
                                                         : modelData.key === "speak" ? controller.speakToChat
                                                         : controller.adaptiveVolume

                                Layout.fillWidth: true
                                Layout.preferredHeight: 112
                                hovered: featHover.hovered
                                active: on
                                HoverHandler { id: featHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 20
                                    anchors.rightMargin: 20
                                    spacing: 15

                                    Rectangle {
                                        Layout.preferredWidth: 44
                                        Layout.preferredHeight: 44
                                        radius: 14
                                        color: featCard.on ? Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.18) : window.surfaceSunk
                                        border.width: 1
                                        border.color: featCard.on ? Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.5) : window.line
                                        Behavior on color { ColorAnimation { duration: window.tBase } }
                                        Behavior on border.color { ColorAnimation { duration: window.tBase } }

                                        Glyph {
                                            anchors.centerIn: parent
                                            path: featCard.modelData.glyph
                                            size: 21
                                            color: featCard.on ? window.accentSoft : window.txtFaint
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3
                                        Text {
                                            text: featCard.modelData.title
                                            color: window.txt
                                            font.pixelSize: 15
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: featCard.modelData.desc
                                            color: window.txtFaint
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    NeoSwitch {
                                        checked: featCard.on
                                        onToggled: {
                                            if (featCard.modelData.key === "dsee") controller.setDsee(checked)
                                            else if (featCard.modelData.key === "speak") controller.setSpeakToChat(checked)
                                            else controller.setAdaptiveVolume(checked)
                                        }
                                    }
                                }
                            }
                        }

                        // Auto power-off — a choice, not a toggle
                        Card {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 112
                            hovered: powerHover.hovered
                            HoverHandler { id: powerHover }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 20
                                anchors.rightMargin: 20
                                spacing: 15

                                Rectangle {
                                    Layout.preferredWidth: 44
                                    Layout.preferredHeight: 44
                                    radius: 14
                                    color: window.surfaceSunk
                                    border.width: 1
                                    border.color: window.line
                                    Glyph {
                                        anchors.centerIn: parent
                                        path: window.icons.power
                                        size: 21
                                        color: window.txtFaint
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        text: "Auto Power-Off"
                                        color: window.txt
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: "Shut down after idle or removal"
                                        color: window.txtFaint
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                ComboBox {
                                    id: powerCombo
                                    implicitWidth: 134
                                    implicitHeight: 38
                                    model: ["Off", "5 Minutes", "15 Minutes", "30 Minutes", "1 Hour", "3 Hours"]
                                    currentIndex: controller.autoPowerOff
                                    onActivated: controller.setAutoPowerOff(index)

                                    background: Rectangle {
                                        radius: 11
                                        color: powerCombo.hovered ? window.surfaceHi : window.surfaceSunk
                                        border.width: 1
                                        border.color: powerCombo.hovered ? window.lineHi : window.line
                                        Behavior on color { ColorAnimation { duration: window.tFast } }
                                    }

                                    contentItem: Text {
                                        leftPadding: 13
                                        rightPadding: 28
                                        text: powerCombo.displayText
                                        color: window.txt
                                        font.pixelSize: 12
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }

                                    indicator: Glyph {
                                        x: powerCombo.width - width - 12
                                        y: powerCombo.height / 2 - height / 2
                                        size: 14
                                        color: window.txtFaint
                                        path: window.icons.chevron
                                        rotation: powerCombo.popup.visible ? 180 : 0
                                        Behavior on rotation { NumberAnimation { duration: window.tBase } }
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ==================================================
            // 5 · DEVICE SWITCHER
            // ==================================================
            ViewPage {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 36
                    spacing: 22

                    ColumnLayout {
                        spacing: 5
                        Eyebrow { text: "Easy Switch" }
                        Text {
                            text: "Paired Devices"
                            color: window.txt
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.6
                        }
                        Text {
                            text: "Hand the connection to another set without re-pairing."
                            color: window.txtDim
                            font.pixelSize: 13
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 11

                        Repeater {
                            model: controller.pairedDevices

                            delegate: Card {
                                id: devCard
                                required property var modelData
                                readonly property bool current: modelData.name === controller.deviceName

                                Layout.fillWidth: true
                                Layout.preferredHeight: 84
                                active: current
                                hovered: devHover.hovered
                                scale: (devHover.hovered && !current) ? 1.006 : 1.0
                                Behavior on scale { NumberAnimation { duration: window.tBase } }

                                HoverHandler { id: devHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 20
                                    anchors.rightMargin: 20
                                    spacing: 16

                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 48
                                        radius: 15
                                        color: devCard.current ? Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.18) : window.surfaceSunk
                                        border.width: 1
                                        border.color: devCard.current ? Qt.rgba(window.accent.r, window.accent.g, window.accent.b, 0.5) : window.line

                                        Glyph {
                                            anchors.centerIn: parent
                                            path: window.icons.headphones
                                            size: 22
                                            color: devCard.current ? window.accentSoft : window.txtFaint
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3
                                        Text {
                                            text: devCard.modelData.name
                                            color: window.txt
                                            font.pixelSize: 15
                                            font.weight: Font.DemiBold
                                        }
                                        RowLayout {
                                            spacing: 7
                                            Rectangle {
                                                Layout.preferredWidth: 6
                                                Layout.preferredHeight: 6
                                                radius: 3
                                                color: devCard.current ? window.success : window.txtFaint
                                            }
                                            Text {
                                                text: devCard.current ? "Connected" : "Available"
                                                color: devCard.current ? window.success : window.txtFaint
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                            }
                                            Text {
                                                text: "·  " + devCard.modelData.address
                                                color: window.txtFaint
                                                font.pixelSize: 11
                                            }
                                        }
                                    }

                                    PillButton {
                                        // Fixed width so the action column lines
                                        // up down the list regardless of label.
                                        Layout.preferredWidth: 132
                                        Layout.alignment: Qt.AlignVCenter
                                        implicitHeight: 40
                                        text: devCard.current ? "Active" : "Connect"
                                        glyphPath: devCard.current ? "" : window.icons.swap
                                        active: devCard.current
                                        enabled: !devCard.current
                                        opacity: enabled ? 1.0 : 0.8
                                        onClicked: controller.connectDevice(devCard.modelData.address, devCard.modelData.name)
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
}
