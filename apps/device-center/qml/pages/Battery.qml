import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../components"

ViewPage {
    id: batteryPage
    // Window shown by the chart: 24 hours or 7 days.
    property int rangeHours: 24
    readonly property var locale: Qt.locale(controller.currentLanguage)

    function formatSessionStart(ms) {
        var d = new Date(ms)
        var sameDay = d.toDateString() === new Date().toDateString()
        return locale.toString(d, sameDay ? "HH:mm" : "ddd HH:mm")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        anchors.topMargin: 22
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SectionTitle { appWindow: batteryPage.appWindow;
                Layout.fillWidth: true
                eyebrow: appWindow.tr("battery_eyebrow")
                title: appWindow.tr("battery_title")
                subtitle: appWindow.tr("battery_subtitle")
            }

            PillButton { appWindow: batteryPage.appWindow;
                compact: true
                text: appWindow.tr("battery_range_24h")
                active: batteryPage.rangeHours === 24
                onClicked: batteryPage.rangeHours = 24
            }
            PillButton { appWindow: batteryPage.appWindow;
                compact: true
                text: appWindow.tr("battery_range_7d")
                active: batteryPage.rangeHours === 24 * 7
                onClicked: batteryPage.rangeHours = 24 * 7
            }
        }

        // Live numbers. The estimate is honest: "—" until the
        // session has enough data, and the charging label while
        // the level is going up.
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Repeater {
                model: {
                    var dash = "—"
                    var level = controller.connected && controller.batteryLevel >= 0 ? controller.batteryLevel + "%" : dash
                    var left = !controller.connected ? dash
                             : controller.isCharging ? appWindow.tr("charging")
                             : controller.batteryTimeLeft !== "" ? controller.batteryTimeLeft : appWindow.tr("battery_estimate_pending")
                    var rate = controller.batteryDischargeRate > 0
                             ? appWindow.tr("battery_rate_value").arg(controller.batteryDischargeRate.toFixed(1)) : dash
                    var session = controller.batterySessionStart > 0
                             ? appWindow.tr("battery_session_since").arg(batteryPage.formatSessionStart(controller.batterySessionStart)) : dash
                    return [
                        { k: appWindow.tr("battery"), v: level, accent: controller.isCharging },
                        { k: appWindow.tr("time_left"), v: left, accent: false },
                        { k: appWindow.tr("battery_rate"), v: rate, accent: false },
                        { k: appWindow.tr("battery_session"), v: session, accent: false }
                    ]
                }

                delegate: Rectangle {
                    id: batteryStat
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: statColumn.implicitHeight + 30
                    radius: Theme.cardRadius
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.line

                    ColumnLayout {
                        id: statColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.topMargin: 14
                        spacing: 8
                        Eyebrow { appWindow: batteryPage.appWindow; text: batteryStat.modelData.k }
                        // Short values go on the dot grid; sentences stay in type.
                        DotText {
                            visible: batteryStat.modelData.v.length <= 12
                            text: batteryStat.modelData.v
                            dot: 3.6
                            maxWidth: batteryStat.width - 32
                        }
                        Text {
                            visible: batteryStat.modelData.v.length > 12
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: batteryStat.modelData.v
                            color: Theme.txt
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Card { appWindow: batteryPage.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                RowLayout {
                    spacing: 16
                    Repeater {
                        model: [
                            { label: appWindow.tr("battery_legend_discharging"), tint: Theme.success },
                            { label: appWindow.tr("charging"), tint: Theme.accentSoft }
                        ]
                        delegate: RowLayout {
                            required property var modelData
                            spacing: 6
                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: modelData.tint
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: modelData.label
                                color: Theme.txtDim
                                font.pixelSize: 11
                            }
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                // The chart itself. Plain Canvas: no QtCharts
                // dependency, and the drawing is a hundred lines.
                Canvas {
                    id: batteryChart
                    renderTarget: Canvas.Image
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    property var samples: []
                    readonly property real padL: 38
                    readonly property real padR: 18
                    readonly property real padT: 14
                    readonly property real padB: 28

                    function reload() {
                        var now = Date.now()
                        samples = controller.batterySamples(now - batteryPage.rangeHours * 3600 * 1000)
                        requestPaint()
                    }

                    Component.onCompleted: reload()
                    onVisibleChanged: if (visible) reload()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Connections {
                        target: batteryPage
                        function onRangeHoursChanged() { batteryChart.reload() }
                    }
                    Connections {
                        target: controller
                        function onBatteryHistoryChanged() { batteryChart.reload() }
                        function onLanguageChanged() { batteryChart.requestPaint() }
                    }
                    // "Now" keeps moving even when nothing is logged.
                    Timer {
                        interval: 60 * 1000
                        repeat: true
                        running: batteryChart.visible
                        onTriggered: batteryChart.reload()
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        var w = width - padL - padR
                        var h = height - padT - padB
                        if (w <= 0 || h <= 0) return
                        var now = Date.now()
                        var range = batteryPage.rangeHours * 3600 * 1000
                        var t0 = now - range
                        var X = function(t) { return padL + (t - t0) / range * w }
                        var Y = function(level) { return padT + (1 - level / 100) * h }
                        var rgba = function(c, a) { return Qt.rgba(c.r, c.g, c.b, a).toString() }

                        ctx.font = "10px \"" + Qt.application.font.family + "\""
                        ctx.textBaseline = "middle"

                        // Horizontal grid with percentages.
                        ctx.lineWidth = 1
                        for (var p = 0; p <= 100; p += 25) {
                            var y = Math.round(Y(p)) + 0.5
                            ctx.strokeStyle = p === 0 ? Theme.lineHi.toString() : Theme.line.toString()
                            ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(padL + w, y); ctx.stroke()
                            ctx.fillStyle = Theme.txtFaint.toString()
                            ctx.textAlign = "right"
                            ctx.fillText(p + "%", padL - 8, y)
                        }

                        // Vertical ticks: every 6 hours on the day
                        // view, each midnight on the week view.
                        ctx.textAlign = "center"
                        ctx.textBaseline = "top"
                        var week = batteryPage.rangeHours > 24
                        var tick = new Date(t0)
                        if (week) tick.setHours(0, 0, 0, 0); else tick.setMinutes(0, 0, 0)
                        for (var guard = 0; guard < 200; ++guard) {
                            if (week) tick.setDate(tick.getDate() + 1); else tick.setHours(tick.getHours() + 1)
                            var tt = tick.getTime()
                            if (tt > now) break
                            if (!week && tick.getHours() % 6 !== 0) continue
                            var x = Math.round(X(tt)) + 0.5
                            ctx.strokeStyle = Theme.line.toString()
                            ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x, padT + h); ctx.stroke()
                            ctx.fillStyle = Theme.txtFaint.toString()
                            ctx.fillText(batteryPage.locale.toString(tick, week ? "ddd d" : "HH:mm"), x, padT + h + 8)
                        }
                        ctx.fillStyle = Theme.txtDim.toString()
                        ctx.textAlign = "right"
                        ctx.fillText(appWindow.tr("battery_now"), padL + w, padT + h + 8)

                        // Split the log into runs of one colour:
                        // a disconnect ends a run, a change of the
                        // charging flag starts a new one from the
                        // previous point so the line stays joined.
                        var runs = []
                        var run = null
                        for (var i = 0; i < samples.length; ++i) {
                            var s = samples[i]
                            if (s.level < 0) continue
                            if (s.event === "disconnected") {
                                if (run) { run.pts.push(s); runs.push(run); run = null }
                                continue
                            }
                            if (!run || run.charging !== s.charging) {
                                var prev = run ? run.pts[run.pts.length - 1] : null
                                if (run) runs.push(run)
                                run = { charging: s.charging, pts: prev ? [prev] : [] }
                            }
                            run.pts.push(s)
                        }
                        var endPoint = null
                        if (run) {
                            var last = run.pts[run.pts.length - 1]
                            if (controller.connected) {
                                endPoint = { t: now, level: last.level }
                                run.pts.push(endPoint)
                            }
                            runs.push(run)
                        }

                        ctx.save()
                        ctx.beginPath(); ctx.rect(padL, padT - 4, w, h + 8); ctx.clip()
                        ctx.lineWidth = 2
                        ctx.lineJoin = "round"
                        ctx.lineCap = "round"
                        for (var r = 0; r < runs.length; ++r) {
                            var pts = runs[r].pts
                            if (pts.length < 2) continue
                            var tint = runs[r].charging ? Theme.accentSoft : Theme.success
                            // Soft fill down to the axis, then the line on top.
                            ctx.beginPath()
                            ctx.moveTo(X(pts[0].t), Y(0))
                            for (var k = 0; k < pts.length; ++k) ctx.lineTo(X(pts[k].t), Y(pts[k].level))
                            ctx.lineTo(X(pts[pts.length - 1].t), Y(0))
                            ctx.closePath()
                            ctx.fillStyle = rgba(tint, 0.10)
                            ctx.fill()
                            ctx.beginPath()
                            ctx.moveTo(X(pts[0].t), Y(pts[0].level))
                            for (var m = 1; m < pts.length; ++m) ctx.lineTo(X(pts[m].t), Y(pts[m].level))
                            ctx.strokeStyle = tint.toString()
                            ctx.stroke()
                        }
                        ctx.restore()

                        if (endPoint) {
                            var tintNow = controller.isCharging ? Theme.accentSoft : Theme.success
                            ctx.beginPath()
                            ctx.arc(X(endPoint.t), Y(endPoint.level), 4, 0, Math.PI * 2)
                            ctx.fillStyle = tintNow.toString()
                            ctx.fill()
                            ctx.beginPath()
                            ctx.arc(X(endPoint.t), Y(endPoint.level), 7, 0, Math.PI * 2)
                            ctx.strokeStyle = rgba(tintNow, 0.35)
                            ctx.lineWidth = 2
                            ctx.stroke()
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: batteryChart.samples.length === 0
                        textFormat: Text.PlainText
                        text: appWindow.tr("battery_no_data")
                        color: Theme.txtFaint
                        font.pixelSize: 13
                    }
                }
            }
        }

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: appWindow.tr("battery_estimate_hint")
            color: Theme.txtFaint
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }
}
