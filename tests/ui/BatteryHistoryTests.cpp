#include <QtTest>
#include "BatteryHistory.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <memory>
using namespace sony::devicecenter;

namespace {
constexpr qint64 kMinute = 60 * 1000;
constexpr qint64 kHour = 60 * kMinute;
constexpr qint64 kDay = 24 * kHour;
// A fixed "now" keeps the numbers below readable; nothing in the class
// consults the wall clock.
constexpr qint64 kT0 = 1'700'000'000'000;
const QString kAddress = "CC:98:8B:00:11:22";
}

class BatteryHistoryTests : public QObject {
    Q_OBJECT
    // Fresh storage for every test: the log persists by design, so a shared
    // directory would leak one test's samples into the next.
    std::unique_ptr<QTemporaryDir> _dir;
    QString dir() const { return _dir->path(); }

    // Discharging session: connected at 100%, then one point per percent.
    static void discharge(BatteryHistory& h, qint64 from, int startLevel, int steps, qint64 stepMs) {
        h.observe(from, true, startLevel, false);
        for (int i = 1; i <= steps; ++i) h.observe(from + i * stepMs, true, startLevel - i, false);
    }

private slots:
    void init() { _dir = std::make_unique<QTemporaryDir>(); }
    void cleanup() { _dir.reset(); }

    void recordsOnlyChanges() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        QSignalSpy changed(&h, &BatteryHistory::changed);
        for (int i = 0; i < 10; ++i) h.observe(kT0 + i * 500, true, 87, false);
        QCOMPARE(h.samples().size(), 1);
        QCOMPARE(h.samples()[0].event, BatteryHistory::Event::Connected);
        QCOMPARE(h.samples()[0].level, 87);
        h.observe(kT0 + 10 * kMinute, true, 86, false);
        QCOMPARE(h.samples().size(), 2);
        QCOMPARE(h.samples()[1].event, BatteryHistory::Event::Level);
        // A charger going on at the same level is a change too.
        h.observe(kT0 + 11 * kMinute, true, 86, true);
        QCOMPARE(h.samples().size(), 3);
        QVERIFY(h.samples()[2].charging);
        QCOMPARE(changed.count(), 3);
    }

    void marksConnectAndDisconnect() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        // Nothing to log while the device is away or has no reading yet.
        h.observe(kT0, false, -1, false);
        h.observe(kT0 + 1, true, -1, false);
        QVERIFY(h.samples().isEmpty());
        h.observe(kT0 + 2, true, 80, false);
        h.observe(kT0 + kMinute, false, -1, false);
        h.observe(kT0 + 2 * kMinute, false, -1, false);
        h.observe(kT0 + kHour, true, 75, false);
        QCOMPARE(h.samples().size(), 3);
        QCOMPARE(h.samples()[1].event, BatteryHistory::Event::Disconnected);
        QCOMPARE(h.samples()[1].level, 80);   // carries the last known level for the chart
        QCOMPARE(h.samples()[2].event, BatteryHistory::Event::Connected);
        QCOMPARE(h.samples()[2].level, 75);
    }

    void noDeviceMeansNothingIsRecorded() {
        BatteryHistory h(dir());
        h.observe(kT0, true, 80, false);
        QVERIFY(h.samples().isEmpty());
        QVERIFY(h.filePath().isEmpty());
    }

    void estimateNeedsADropAndSomeTime() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        QVERIFY(!h.estimate(kT0).valid);
        h.observe(kT0, true, 100, false);
        QVERIFY2(!h.estimate(kT0 + kHour).valid, "one sample is not a rate");
        // The connect sample sits at an unknown fraction, so the first level
        // change is only the anchor, not yet a drop.
        h.observe(kT0 + 10 * kMinute, true, 99, false);
        QVERIFY2(!h.estimate(kT0 + 10 * kMinute).valid, "one level change is an anchor, not a rate");
        h.observe(kT0 + 20 * kMinute, true, 98, false);
        const auto e = h.estimate(kT0 + 20 * kMinute);
        QVERIFY2(e.valid, "one whole percent between two reported changes is a rate");
        QCOMPARE(e.sessionStartMs, kT0);
        // 1% over 10 minutes = 6%/h; 98% lasts 980 minutes.
        QCOMPARE(e.percentPerHour, 6.0);
        QCOMPARE(e.remainingMs / kMinute, 980);
        // Time keeps running between samples.
        QCOMPARE(h.estimate(kT0 + 25 * kMinute).remainingMs / kMinute, 975);
        h.observe(kT0 + 30 * kMinute, true, 97, false);
        QCOMPARE(h.estimate(kT0 + 30 * kMinute).remainingMs / kMinute, 970);
    }

    void restartsMeasureBetweenLevelChangesOnly() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 3, 10 * kMinute);   // 6%/h, 57 at +30 min
        // Relaunched at the same level five minutes later: the marker is a
        // resumed session, and it must not pass as the end of the measured
        // span (57 at +35 min would read as a slower rate).
        BatteryHistory again(dir());
        again.setDevice(kAddress);
        again.observe(kT0 + 35 * kMinute, true, 57, false);
        const auto e = again.estimate(kT0 + 35 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, kT0);
        QCOMPARE(e.percentPerHour, 6.0);
        // 57% at 6%/h is 570 min, minus the five minutes since the change.
        QCOMPARE(e.remainingMs / kMinute, 565);
    }

    void tooShortASessionIsNotTrusted() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        // 3% in three minutes: the drop is there, the span is not.
        discharge(h, kT0, 100, 4, kMinute);
        QVERIFY(!h.estimate(kT0 + 4 * kMinute).valid);
        h.observe(kT0 + 10 * kMinute, true, 95, false);
        QVERIFY(h.estimate(kT0 + 10 * kMinute).valid);
    }

    void estimateRestartsWhenChargingStops() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 10, 10 * kMinute);   // 6%/h, ends at 50% after 100 min
        QVERIFY(h.estimate(kT0 + 100 * kMinute).valid);
        // On the charger: no estimate at all.
        h.observe(kT0 + 2 * kHour, true, 50, true);
        h.observe(kT0 + 2 * kHour + 5 * kMinute, true, 55, true);
        QVERIFY(!h.estimate(kT0 + 2 * kHour + 5 * kMinute).valid);
        // Off the charger at 100%: the old rate must not be reused.
        const qint64 off = kT0 + 4 * kHour;
        h.observe(off, true, 100, false);
        QVERIFY(!h.estimate(off + kHour).valid);
        h.observe(off + 30 * kMinute, true, 99, false);
        h.observe(off + 90 * kMinute, true, 97, false);
        const auto e = h.estimate(off + 90 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, off);
        QCOMPARE(e.percentPerHour, 2.0);
        QCOMPARE(e.remainingMs / kMinute, 97 * 30);
    }

    void estimateIsUnknownWhileDisconnected() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 10, 10 * kMinute);
        h.observe(kT0 + 2 * kHour, false, -1, false);
        QVERIFY(!h.estimate(kT0 + 2 * kHour).valid);
    }

    void shortReconnectKeepsTheSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);   // 6%/h
        // A one-minute Bluetooth hiccup.
        h.observe(kT0 + 61 * kMinute, false, -1, false);
        h.observe(kT0 + 62 * kMinute, true, 54, false);
        h.observe(kT0 + 70 * kMinute, true, 53, false);
        const auto e = h.estimate(kT0 + 70 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, kT0);
        QCOMPARE(e.percentPerHour, 6.0);
    }

    void restartAtTheSameLevelKeepsTheSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);   // 6%/h, last point 54 at +60 min
        // The app was relaunched: no disconnect marker, a fresh connect at
        // the level the log already ends with.
        {
            BatteryHistory again(dir());
            again.setDevice(kAddress);
            again.observe(kT0 + 61 * kMinute, true, 54, false);
            QCOMPARE(again.samples().last().event, BatteryHistory::Event::Connected);
            again.observe(kT0 + 70 * kMinute, true, 53, false);
            const auto e = again.estimate(kT0 + 70 * kMinute);
            QVERIFY(e.valid);
            QCOMPARE(e.sessionStartMs, kT0);
            QCOMPARE(e.percentPerHour, 6.0);
        }
        // Relaunched far below the last point: 13% in a minute is no
        // discharge, the charge was spent elsewhere.
        BatteryHistory later(dir());
        later.setDevice(kAddress);
        later.observe(kT0 + 71 * kMinute, true, 40, false);
        QVERIFY(!later.estimate(kT0 + 71 * kMinute).valid);
        QCOMPARE(later.estimate(kT0 + 71 * kMinute).sessionStartMs, kT0 + 71 * kMinute);
    }

    void restartAfterUnseenUseKeepsTheSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);   // 6%/h, 54 at +60 min
        // The app was closed for an hour and a half while the headset played
        // on: 9% down at the relaunch, the same rate as before.
        BatteryHistory again(dir());
        again.setDevice(kAddress);
        again.observe(kT0 + 150 * kMinute, true, 45, false);
        auto e = again.estimate(kT0 + 150 * kMinute);
        QVERIFY2(e.valid, "the earlier changes still count");
        QCOMPARE(e.sessionStartMs, kT0);
        QCOMPARE(e.percentPerHour, 6.0);
        // The next change refines the rate across the gap.
        again.observe(kT0 + 160 * kMinute, true, 44, false);
        e = again.estimate(kT0 + 160 * kMinute);
        QCOMPARE(e.sessionStartMs, kT0);
        QCOMPARE(e.percentPerHour, 15.0 * 60 / 150);
    }

    void restartAtAHigherLevelStartsANewSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);
        // Charged while the app was closed. Headphones that cannot play on
        // the charger never log a charging sample, so the rise is the only clue.
        BatteryHistory again(dir());
        again.setDevice(kAddress);
        const qint64 back = kT0 + 3 * kHour;
        again.observe(back, true, 100, false);
        QVERIFY(!again.estimate(back).valid);
        QCOMPARE(again.estimate(back).sessionStartMs, back);
    }

    void restartAfterALongGapStartsANewSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);
        // Same level a night later: the headset sat switched off, and the
        // hours in between would make the old rate meaningless.
        BatteryHistory again(dir());
        again.setDevice(kAddress);
        const qint64 back = kT0 + 12 * kHour;
        again.observe(back, true, 54, false);
        QCOMPARE(again.estimate(back).sessionStartMs, back);
    }

    void implausiblySlowRateIsNotTrusted() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        // 1% in three hours can only mean the headset was off for most of
        // it; "300 hours left" helps nobody.
        h.observe(kT0, true, 60, false);
        h.observe(kT0 + 10 * kMinute, true, 59, false);
        h.observe(kT0 + 3 * kHour, true, 58, false);
        QVERIFY(!h.estimate(kT0 + 3 * kHour).valid);
    }

    // A log recorded by a WH-1000XM5 on Windows during an evening of
    // development: the app was relaunched many times, so most markers are
    // "connected" with no "disconnected" before them, and the charge that
    // happened after 01:03 never shows because these headphones cannot play
    // while charging.
    static void writeXm5Log(const QString& dir, int count = 14) {
        static const char* const samples[] = {
            R"({"t":1789587216150,"level":59,"charging":false,"event":"connected"})",     // 22:33 first launch
            R"({"t":1789587376622,"level":59,"charging":false,"event":"connected"})",     // 22:36 relaunch
            R"({"t":1789587829646,"level":59,"charging":false,"event":"connected"})",     // 22:43 relaunch
            R"({"t":1789587890606,"level":58,"charging":false,"event":"level"})",         // 22:44
            R"({"t":1789588185850,"level":58,"charging":false,"event":"connected"})",     // 22:49 relaunch
            R"({"t":1789588474983,"level":58,"charging":false,"event":"connected"})",     // 22:54 relaunch
            R"({"t":1789594130300,"level":53,"charging":false,"event":"connected"})",     // 00:28 relaunch, 94 min unseen
            R"({"t":1789594632311,"level":52,"charging":false,"event":"level"})",         // 00:37
            R"({"t":1789595478866,"level":52,"charging":false,"event":"connected"})",     // 00:51 relaunch
            R"({"t":1789595673359,"level":51,"charging":false,"event":"level"})",         // 00:54
            R"({"t":1789596222937,"level":51,"charging":false,"event":"disconnected"})",  // 01:03 headset off
            R"({"t":1789641009471,"level":100,"charging":false,"event":"connected"})",    // 13:30 charged overnight
            R"({"t":1789641101351,"level":100,"charging":false,"event":"connected"})",    // 13:31 relaunch
            R"({"t":1789642092844,"level":100,"charging":false,"event":"disconnected"})", // 13:48 headset off
        };
        QByteArray json = R"({"version":1,"address":"CC:98:8B:00:11:22","samples":[)";
        for (int i = 0; i < count; ++i) json += (i ? "," : "") + QByteArray(samples[i]);
        json += "]}";
        QDir().mkpath(dir + "/battery-history");
        QFile file(dir + "/battery-history/CC-98-8B-00-11-22.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(json);
    }

    void realXm5LogGivesAnEstimate() {
        // As the app stood at 00:54, right after the change to 51%: the
        // session runs from the first launch at 22:33, across every relaunch
        // and the 94-minute gap in which 5% went (3.2%/h, in use).
        writeXm5Log(dir(), 10);
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        QCOMPARE(h.samples().size(), 10);
        const qint64 at0054 = 1789595673359;
        const auto e = h.estimate(at0054);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, 1789587216150);
        // 58 -> 51 between the two changes at 22:44:50 and 00:54:33.
        QVERIFY2(e.percentPerHour > 3.2 && e.percentPerHour < 3.3, qPrintable(QString::number(e.percentPerHour)));
        QCOMPARE(e.remainingMs / kHour, 15);
    }

    void realXm5LogAfterChargingHasNoEstimateYet() {
        // Back at 100% after a night on the charger: a new session from
        // 13:30 (the relaunch at 13:31 resumes it) with no level change yet,
        // which is what the card showed on the day.
        writeXm5Log(dir(), 12);
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        const qint64 at1331 = 1789641101351;
        h.observe(at1331, true, 100, false);   // the relaunch that wrote sample 13
        QCOMPARE(h.samples().last().event, BatteryHistory::Event::Connected);
        auto e = h.estimate(at1331 + 5 * kMinute);
        QVERIFY(!e.valid);
        QCOMPARE(e.sessionStartMs, 1789641009471);
        // Carry on as the headset would have: the first change is still only
        // the anchor, the second one gives the rate.
        h.observe(at1331 + 10 * kMinute, true, 99, false);
        QVERIFY(!h.estimate(at1331 + 10 * kMinute).valid);
        h.observe(at1331 + 28 * kMinute, true, 98, false);
        e = h.estimate(at1331 + 28 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, 1789641009471);
        QVERIFY(e.percentPerHour > 3.3 && e.percentPerHour < 3.4);
        // Gone at 13:48: nothing to estimate while the headset is off.
        writeXm5Log(dir(), 14);
        BatteryHistory full(dir());
        full.setDevice(kAddress);
        QVERIFY(!full.estimate(1789642092844 + kMinute).valid);
    }

    void longReconnectStartsANewSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);
        // Away for the night; the level fell while it was elsewhere.
        h.observe(kT0 + 61 * kMinute, false, -1, false);
        const qint64 back = kT0 + 10 * kHour;
        h.observe(back, true, 30, false);
        QVERIFY2(!h.estimate(back).valid, "no data from this session yet");
        h.observe(back + 10 * kMinute, true, 29, false);
        h.observe(back + 40 * kMinute, true, 27, false);
        const auto e = h.estimate(back + 40 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, back);
        QCOMPARE(e.percentPerHour, 4.0);
    }

    void persistsAcrossInstancesPerDevice() {
        {
            BatteryHistory h(dir());
            h.setDevice(kAddress);
            discharge(h, kT0, 90, 3, kMinute);
            h.setDevice("AA:BB:CC:DD:EE:FF");
            h.observe(kT0, true, 40, true);
            QCOMPARE(h.samples().size(), 1);
        }
        QVERIFY(QFile::exists(dir() + "/battery-history/CC-98-8B-00-11-22.json"));
        QVERIFY(QFile::exists(dir() + "/battery-history/AA-BB-CC-DD-EE-FF.json"));
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        QCOMPARE(h.samples().size(), 4);
        QCOMPARE(h.samples()[0].event, BatteryHistory::Event::Connected);
        QCOMPARE(h.samples()[3].level, 87);
        QCOMPARE(h.samples()[3].timeMs, kT0 + 3 * kMinute);
        // A fresh instance starts a new connection marker rather than
        // continuing the old line.
        h.observe(kT0 + 4 * kMinute, true, 87, false);
        QCOMPARE(h.samples().size(), 5);
        QCOMPARE(h.samples()[4].event, BatteryHistory::Event::Connected);

        h.setDevice("AA:BB:CC:DD:EE:FF");
        QCOMPARE(h.samples().size(), 1);
        QVERIFY(h.samples()[0].charging);
    }

    void fileIsCompactJson() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        h.observe(kT0, true, 87, false);
        QFile file(h.filePath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root.value("version").toInt(), 1);
        QCOMPARE(root.value("address").toString(), kAddress);
        const auto sample = root.value("samples").toArray().first().toObject();
        QCOMPARE(static_cast<qint64>(sample.value("t").toDouble()), kT0);
        QCOMPARE(sample.value("level").toInt(), 87);
        QCOMPARE(sample.value("event").toString(), QString("connected"));
    }

    void oldSamplesAreDropped() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        h.observe(kT0 - 200 * kDay, true, 50, false);
        h.observe(kT0 - 100 * kDay, true, 49, false);
        h.observe(kT0, true, 48, false);
        QCOMPARE(h.samples().size(), 2);
        QCOMPARE(h.samples()[0].timeMs, kT0 - 100 * kDay);
    }

    void samplesSinceKeepsOnePriorPointForTheChart() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 90, 5, kHour);
        const auto window = h.samplesSince(kT0 + 3 * kHour + kMinute);
        QCOMPARE(window.size(), 3);   // t+3h (prior), t+4h, t+5h
        const auto first = window.first().toMap();
        QCOMPARE(first.value("t").toLongLong(), kT0 + 3 * kHour);
        QCOMPARE(first.value("level").toInt(), 87);
        QCOMPARE(first.value("event").toString(), QString("level"));
        QCOMPARE(first.value("charging").toBool(), false);
        QCOMPARE(h.samplesSince(kT0 + kDay).size(), 1);
    }

    void demoSeedGivesTheSimulatorAnEstimate() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        h.seedDemoData(kT0);
        QVERIFY(h.samples().size() > 500);
        for (int i = 1; i < h.samples().size(); ++i) QVERIFY(h.samples()[i].timeMs >= h.samples()[i - 1].timeMs);
        const auto e = h.estimate(kT0);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, kT0 - 2 * kHour);
        QVERIFY(e.percentPerHour > 3.0 && e.percentPerHour < 5.0);
        // The simulator's first report matches the seed's last point, so it
        // continues the line instead of starting a new session.
        const auto before = h.samples().size();
        h.observe(kT0 + 1000, true, 87, false);
        QCOMPARE(h.samples().size(), before);
        h.observe(kT0 + 3000, true, 86, false);
        QCOMPARE(h.samples().last().event, BatteryHistory::Event::Level);
    }

    void sanitizesAddresses() {
        QCOMPARE(BatteryHistory::sanitizeAddress("CC:98:8B:00:11:22"), QString("CC-98-8B-00-11-22"));
        QCOMPARE(BatteryHistory::sanitizeAddress("../x\\y z"), QString("xyz"));
    }
};

QTEST_GUILESS_MAIN(BatteryHistoryTests)
#include "BatteryHistoryTests.moc"
