#include <QtTest>
#include "DeviceCenterController.h"
#include "TrayController.h"
#include "NotificationController.h"
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QDir>
#include <QSettings>
#include "sony/core/DeviceService.h"
#include "sony/core/SimulatedDevice.h"
#include "sony/protocol/FrameCodec.h"
#include "../support/ReplyTransport.h"
#include <chrono>
using namespace sony;
using namespace sony::devicecenter;
class SlowService : public core::IDeviceService {
public:
    void tick() override { std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
    std::vector<core::DiscoveredDevice> discoverDevices() override { return {}; }
    void connect(const transport::DeviceAddress&, std::string_view) override {}
    void disconnect() noexcept override {}
    bool isConnected() const noexcept override { return false; }
    core::SonyDevice* activeDevice() noexcept override { return nullptr; }
    protocol::DeviceStateSnapshot snapshot() const override { return std::make_shared<const protocol::DeviceState>(); }
};
class DeviceControllerTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QQuickStyle::setStyle("Basic"); QStandardPaths::setTestModeEnabled(true); }
    void extractedPagesLoad_data() {
        QTest::addColumn<QString>("model");
        QTest::addColumn<QString>("language");
        QTest::addColumn<QSize>("size");
        for (const auto& model : {"WH-1000XM5", "WF-1000XM5"})
            for (const auto& language : {"en", "ru"})
                for (const auto size : {QSize(980, 660), QSize(1600, 1000)}) {
                    const auto name = QString("%1-%2-%3").arg(model, language).arg(size.width());
                    QTest::newRow(qPrintable(name)) << QString(model) << QString(language) << size;
                }
    }
    void extractedPagesLoad() {
        QFETCH(QString, model);
        QFETCH(QString, language);
        QFETCH(QSize, size);
        auto simulated = core::createSimulatedDevice(model.toStdString());
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        const auto previousLanguage = controller.currentLanguage();
        controller.setLanguage(language);
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError>& errors) {
            for (const auto& error : errors) warnings.append(error.toString());
        });
        engine.rootContext()->setContextProperty("controller", &controller);
        engine.rootContext()->setContextProperty("trayAvailable", false);
        engine.rootContext()->setContextProperty("startHidden", false);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        controller.setLanguage(previousLanguage);
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join("\n")));
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(window);
        controller.setLanguage(language);
        window->resize(size);
        const auto previousTheme = controller.themeMode();
        const auto previousAnimations = controller.animationsEnabled();
        controller.setThemeMode("light");
        QTest::qWait(20);
        QCOMPARE(window->color(), QColor("#EDEDED"));
        const auto output = qEnvironmentVariable("SONY_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            window->setProperty("navIndex", 6);
            QTest::qWait(400);
            QVERIFY(window->grabWindow().save(QString("%1/light-%2-%3-%4.png").arg(output, model, language).arg(size.width())));
        }
        const bool previousSmoothing = controller.iconAntialiasing();
        controller.setIconAntialiasing(false);
        QCOMPARE(QSettings("SonyBridge", "SonyDeviceCenter").value("iconAntialiasing").toBool(), false);
        controller.setIconAntialiasing(previousSmoothing);
        controller.setThemeMode("dark");
        QTest::qWait(20);
        QCOMPARE(window->color(), QColor("#0C0C0C"));
        controller.setAnimationsEnabled(false);
        QCOMPARE(QSettings("SonyBridge", "SonyDeviceCenter").value("animationsEnabled").toBool(), false);
        controller.setThemeMode("invalid");
        QCOMPARE(controller.themeMode(), QString("dark"));
        controller.setAnimationsEnabled(previousAnimations);
        for (int page = 0; page < 7; ++page) {
            QVERIFY(window->setProperty("navIndex", page));
            const auto screenshotDirectory = qEnvironmentVariable("SONY_UI_SCREENSHOTS");
            QTest::qWait(screenshotDirectory.isEmpty() ? 30 : 400);
            if (!screenshotDirectory.isEmpty()) {
                QDir().mkpath(screenshotDirectory);
                const auto path = QString("%1/%2-%3-%4-page%5.png")
                    .arg(screenshotDirectory, model, language).arg(size.width()).arg(page);
                QVERIFY(window->grabWindow().save(path));
            }
        }
        const auto* smoothingSwitch = window->findChild<QObject*>("iconSmoothingSwitch");
        QVERIFY(smoothingSwitch);
        QCOMPARE(smoothingSwitch->property("checked").toBool(), controller.iconAntialiasing());
        const auto* animationsSwitch = window->findChild<QObject*>("animationsSwitch");
        QVERIFY(animationsSwitch);
        QCOMPARE(animationsSwitch->property("checked").toBool(), controller.animationsEnabled());
        // Typing into a dot-matrix value sends the clamped number to the device.
        if (controller.hasClearBass()) {
            auto* bassValue = window->findChild<QObject*>("clearBassValue");
            QVERIFY(bassValue);
            QVERIFY(QMetaObject::invokeMethod(bassValue, "apply", Q_ARG(QVariant, QVariant("37"))));
            QTRY_COMPARE_WITH_TIMEOUT(controller.clearBass(), 10, 3000);
            QVERIFY(QMetaObject::invokeMethod(bassValue, "apply", Q_ARG(QVariant, QVariant("-3"))));
            QTRY_COMPARE_WITH_TIMEOUT(controller.clearBass(), -3, 3000);
            QTRY_COMPARE_WITH_TIMEOUT(bassValue->property("value").toInt(), -3, 3000);
        }
        controller.setThemeMode(previousTheme);
        controller.setLanguage(previousLanguage);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join("\n")));
    }
    void startupDoesNotBlockGui() {
        auto service = std::make_shared<SlowService>();
        QElapsedTimer elapsed; elapsed.start();
        DeviceCenterController controller(nullptr, service);
        QVERIFY(elapsed.elapsed() < 100);
        QVERIFY(controller.busy());
        bool guiTick = false;
        QTimer::singleShot(10, [&] { guiTick = true; });
        QTRY_VERIFY_WITH_TIMEOUT(guiTick, 100);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QCOMPARE(controller.batteryLevel(), -1);
        QCOMPARE(controller.noiseControlMode(), QString("unknown"));
        QCOMPARE(controller.codec(), QString("Unknown"));
    }
    void earbudsExposePerSideAndCaseBattery() {
        auto simulated = core::createSimulatedDevice("WF-1000XM5");
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QVERIFY(controller.hasDualBattery());
        QCOMPARE(controller.batteryLeft(), 81);
        QCOMPARE(controller.batteryRight(), 79);
        QCOMPARE(controller.batteryCase(), 64);
        QCOMPARE(controller.batteryLevel(), 79); // the weaker side drives the ring
    }
    void overEarHasNoDualBattery() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QVERIFY(!controller.hasDualBattery());
        QCOMPARE(controller.batteryLevel(), 87);
        QCOMPARE(controller.batteryCase(), -1);
    }
    void rapidSliderValuesCoalesceToTheLastOne() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        simulated.transport->clearSent();
        for (int level = 5; level <= 15; ++level) controller.setAmbient(level, false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && controller.ambientLevel() == 15, 5000);
        int noiseWrites = 0; int lastLevel = -1;
        for (const auto& raw : simulated.transport->sentFrames()) {
            const auto frame = protocol::FrameCodec::decode(raw);
            if (frame.payload.size() >= 7 && frame.payload[0] == 0x68) { ++noiseWrites; lastLevel = frame.payload[6]; }
        }
        QCOMPARE(lastLevel, 15);
        QVERIFY2(noiseWrites <= 3, qPrintable(QString("expected the drag to coalesce, got %1 writes").arg(noiseWrites)));
    }
    void ambientLevelSurvivesNoiseCancelling() {
        // Outside ambient mode the protocol reports level 0. The controller
        // must keep the last real level so "back to ambient" restores it
        // instead of asking for 0 (rejected) or 1 (wrong).
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        // The level is a persisted user setting; put it back afterwards.
        const auto previousLevel = QSettings("SonyBridge", "SonyDeviceCenter").value("ambientLevel", 10);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.noiseControlMode(), QString("cancelling"));
        auto settle = [&] { QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000); QVERIFY2(controller.lastError().isEmpty(), qPrintable(controller.lastError())); };
        controller.setAmbient(14, false); settle();
        QCOMPARE(controller.noiseControlMode(), QString("ambient"));
        QCOMPARE(controller.ambientLevel(), 14);
        controller.setAnc(true); settle();
        QCOMPARE(controller.noiseControlMode(), QString("cancelling"));
        QVERIFY2(controller.ambientLevel() == 14, "remembered while the device reports 0");
        controller.setAmbient(controller.ambientLevel(), false); settle();
        QCOMPARE(controller.noiseControlMode(), QString("ambient"));
        QCOMPARE(controller.ambientLevel(), 14);
        // Persisted, so an app restart starts from it too.
        QSettings settings("SonyBridge", "SonyDeviceCenter");
        QCOMPARE(settings.value("ambientLevel").toInt(), 14);
        settings.setValue("ambientLevel", previousLevel);
    }
    void trayIconReflectsBatteryAndConnection() {
        // Rendering is pure: no tray needed, so it runs headless too.
        auto pixel = [](const QIcon& icon, int x, int y) { return icon.pixmap(64, 64).toImage().pixelColor(x, y); };
        const auto full = TrayController::renderIcon(87, false, true);
        const auto low = TrayController::renderIcon(10, false, true);
        const auto gone = TrayController::renderIcon(-1, false, false);
        QVERIFY(!full.isNull());
        // Top of the ring at 12 o'clock is inside the filled arc for any level > 0.
        QCOMPARE(pixel(full, 32, 3).name(), QColor("#2DD4A7").name());
        QCOMPARE(pixel(low, 32, 3).name(), QColor("#FF5A5F").name());
        QCOMPARE(pixel(gone, 32, 3).name(), QColor("#3A3D48").name());
        QCOMPARE(pixel(TrayController::renderIcon(50, true, true), 32, 3).name(), QColor("#7C8CFF").name());
    }
    void notificationsFireOnceAtEachEdge() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        controller.setNotifyCharged(true);
        controller.setLowBatteryThreshold(20);
        TrayController tray(controller);
        NotificationController notifications(controller, tray);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && controller.batteryLevel() == 87, 5000);
        // Starting next to connected headphones is a baseline, not an event.
        QCOMPARE(notifications.messageCount(), 0);

        auto settle = [&](int level) { QTRY_COMPARE_WITH_TIMEOUT(controller.batteryLevel(), level, 3000); QTest::qWait(50); };
        simulated.transport->setBattery(18, false); settle(18);
        QCOMPARE(notifications.messageCount(), 1);
        QVERIFY(notifications.lastMessage().contains("18"));
        simulated.transport->setBattery(17, false); settle(17);
        QVERIFY2(notifications.messageCount() == 1, "still low: must not repeat");
        simulated.transport->setBattery(9, false); settle(9);
        QVERIFY2(notifications.messageCount() == 2, "critical step announces again");
        QVERIFY(notifications.lastMessage().contains("9"));
        simulated.transport->setBattery(100, true); settle(100);
        QCOMPARE(notifications.messageCount(), 3);
        QVERIFY(notifications.lastMessage().contains(controller.t("notify_charged")));
        QTest::qWait(1200);
        QVERIFY2(notifications.messageCount() == 3, "charged: polling must not repeat it");

        controller.setNotifyConnection(true);
        service->activeDevice()->powerOff();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isConnected(), 5000);
        QTest::qWait(50);
        QCOMPARE(notifications.messageCount(), 4);
        QVERIFY(notifications.lastMessage().contains(controller.t("notify_disconnected")));
    }
    void batteryLogFollowsTheSimulatedDevice() {
        QTemporaryDir dir;
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service, dir.path());
        QSignalSpy history(&controller, &DeviceCenterController::batteryHistoryChanged);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && controller.batteryLevel() == 87, 5000);
        auto& log = controller.batteryHistory();
        QCOMPARE(log.device(), QString::fromStdString(simulated.address));
        // Polling twice a second adds nothing while the level holds.
        QTest::qWait(1200);
        QCOMPARE(log.samples().size(), 1);
        QCOMPARE(log.samples()[0].event, BatteryHistory::Event::Connected);
        QCOMPARE(log.samples()[0].level, 87);
        QCOMPARE(controller.batteryMinutesLeft(), -1);
        QCOMPARE(controller.batteryTimeLeft(), QString());

        simulated.transport->setBattery(86, false);
        QTRY_COMPARE_WITH_TIMEOUT(log.samples().size(), 2, 3000);
        QCOMPARE(log.samples()[1].level, 86);
        QVERIFY2(controller.batteryMinutesLeft() == -1, "seconds of data are not an estimate");
        simulated.transport->setBattery(86, true);
        QTRY_COMPARE_WITH_TIMEOUT(log.samples().size(), 3, 3000);
        QVERIFY(log.samples()[2].charging);
        QCOMPARE(controller.batteryDischargeRate(), 0.0);
        QVERIFY(history.count() >= 3);
        QVERIFY(QFile::exists(dir.path() + "/battery-history/CC-98-8B-00-11-22.json"));

        service->activeDevice()->powerOff();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isConnected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(log.samples().size(), 4, 3000);
        QCOMPARE(log.samples()[3].event, BatteryHistory::Event::Disconnected);
    }
    void durationsAreLocalised() {
        auto service = std::make_shared<SlowService>();
        DeviceCenterController controller(nullptr, service);
        // The language is a persisted user setting; put it back afterwards.
        const auto previous = controller.currentLanguage();
        controller.setLanguage("en");
        QCOMPARE(controller.formatDuration(320), QString("5 h 20 min"));
        QCOMPARE(controller.formatDuration(45), QString("45 min"));
        controller.setLanguage("ru");
        QCOMPARE(controller.formatDuration(320), QString::fromUtf8("5 ч 20 мин"));
        controller.setLanguage(previous);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
    }
    void failedActionPreservesConfirmedValue() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        QCOMPARE(controller.noiseControlMode(),QString("cancelling"));
        transport->simulateTimeoutOnSend();
        controller.setNoiseControlOff();
        QVERIFY(controller.busy());
        QCOMPARE(controller.noiseControlMode(),QString("cancelling"));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),3000);
        QVERIFY(!controller.lastError().isEmpty());
        QCOMPARE(controller.noiseControlMode(),QString("cancelling"));
    }
    void notificationsUpdateStateAndBands() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        QCOMPARE(controller.equalizerBands().size(),5);
        QCOMPARE(controller.equalizerBands()[4].toInt(),5);
        transport->notify({0x69,0x17,1,1,1,0,12});
        QTRY_COMPARE_WITH_TIMEOUT(controller.noiseControlMode(),QString("ambient"),1000);
        QCOMPARE(controller.ambientLevel(),12);
    }
    void destructionDrainsWorkerAndCallbacks() {
        auto service = std::make_shared<SlowService>();
        auto controller = std::make_unique<DeviceCenterController>(nullptr,service);
        QTest::qWait(20);
        controller.reset();
        QCoreApplication::processEvents();
    }
};
// A full (widgets) application: the tray icon renderer paints with fonts.
QTEST_MAIN(DeviceControllerTests)
#include "DeviceControllerTests.moc"
