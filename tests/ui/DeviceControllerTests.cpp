#include "sony/core/ControlSettings.h"
#include <QScopeGuard>
#include <QtTest>
#include "DeviceCenterController.h"
#include "DeviceBackend.h"
#include "sony/core/DeviceService.h"
#include "../support/ReplyTransport.h"
#include <chrono>
#include <condition_variable>
using namespace sony;
using namespace sony::devicecenter;
class SlowService : public core::IDeviceService {
public:
    std::atomic<int> ticks{0};
    void tick() override { ++ticks; std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
    std::vector<core::DiscoveredDevice> discoverDevices() override { return {}; }
    void connect(const transport::DeviceAddress&, std::string_view) override {}
    void disconnect() noexcept override {}
    bool isConnected() const noexcept override { return false; }
    core::SonyDevice* activeDevice() noexcept override { return nullptr; }
    protocol::DeviceStateSnapshot snapshot() const override { return std::make_shared<const protocol::DeviceState>(); }
};
class CancellableService : public SlowService {
public:
    std::atomic<bool> entered{false};
    std::mutex mutex;
    std::condition_variable changed;
    bool cancelled{false};
    void tick() override {
        std::unique_lock lock(mutex);
        entered = true;
        changed.wait_for(lock, std::chrono::seconds(2), [this] { return cancelled; });
    }
    void cancelPending() noexcept override {
        std::lock_guard lock(mutex);
        cancelled = true;
        changed.notify_all();
    }
};
auto useConfigHome(const QString& path) {
    const auto previous = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", path.toUtf8());
    return qScopeGuard([previous] {
        if (previous.isNull()) qunsetenv("XDG_CONFIG_HOME");
        else qputenv("XDG_CONFIG_HOME", previous);
    });
}

class DeviceControllerTests : public QObject {
    Q_OBJECT
private slots:
    void blePreferencePersistsAcrossRestart() {
#if defined(SONY_HAS_LINUX_BLUETOOTH)
        QTemporaryDir directory(QDir::currentPath() + "/ble-preference-XXXXXX");
        QVERIFY(directory.isValid());
        const auto config = useConfigHome(directory.path());
        QVERIFY(!core::bleControlEnabled());
        {
            DeviceCenterController controller(nullptr, std::make_shared<SlowService>());
            QVERIFY(controller.showBleControlSetting());
            QVERIFY(!controller.bleControlEnabled());
            controller.setBleControlEnabled(true);
            QVERIFY(controller.bleControlEnabled());
            QVERIFY(core::bleControlEnabled());
        }
        {
            DeviceCenterController controller(nullptr, std::make_shared<SlowService>());
            QVERIFY(controller.bleControlEnabled());
            controller.setBleControlEnabled(false);
            QVERIFY(!controller.bleControlEnabled());
            QVERIFY(!core::bleControlEnabled());
        }
        DeviceCenterController restarted(nullptr, std::make_shared<SlowService>());
        QVERIFY(!restarted.bleControlEnabled());
#else
        QSKIP("Linux-only preference");
#endif
    }
    void invalidBlePreferencesDefaultOff() {
        QTemporaryDir directory(QDir::currentPath() + "/ble-preference-XXXXXX");
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("control.json");
        for (const QByteArray data : {QByteArray("invalid"), QByteArray("[]"), QByteArray("{}"),
                QByteArray("{\"bleControlEnabled\":\"true\"}"), QByteArray("{\"bleControlEnabled\":1}")}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(data), data.size());
            file.close();
            QVERIFY(!core::bleControlEnabled(path.toStdString()));
        }
    }
    void failedPreferenceSaveKeepsPreviousValue() {
#if defined(SONY_HAS_LINUX_BLUETOOTH)
        QTemporaryDir directory(QDir::currentPath() + "/ble-preference-XXXXXX");
        QVERIFY(directory.isValid());
        const auto config = useConfigHome(directory.path());
        QFile obstruction(directory.filePath("sony-device-center"));
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        obstruction.close();
        DeviceCenterController controller(nullptr, std::make_shared<SlowService>());
        controller.setBleControlEnabled(true);
        QVERIFY(!controller.bleControlEnabled());
        QVERIFY(!controller.lastError().isEmpty());
#else
        QSKIP("Linux-only preference");
#endif
    }
    void bleHardwareNoiseControl() {
        const auto address = qEnvironmentVariable("SONY_TEST_BLE_ADDRESS");
        if (address.isEmpty()) QSKIP("Opt-in XM6 BLE hardware test");
        DeviceCenterController controller;
        QTRY_VERIFY_WITH_TIMEOUT(controller.isConnected() && !controller.busy(),10000);
        QCOMPARE(controller.deviceAddress(), address);
        QVERIFY(controller.hasAnc());
        QVERIFY(controller.hasAmbient());
        QVERIFY(!controller.hasEqualizer());
        QCOMPARE(controller.featureStatus().value("autoPowerOff").toMap().value("availability").toString(), QString("unsupported"));
        controller.setAnc(true);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),5000);
        QVERIFY2(controller.lastError().isEmpty(),qPrintable(controller.lastError()));
        QCOMPARE(controller.noiseControlMode(),QString("cancelling"));
        controller.setAmbient(12);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),5000);
        QVERIFY2(controller.lastError().isEmpty(),qPrintable(controller.lastError()));
        QCOMPARE(controller.noiseControlMode(),QString("ambient"));
        QCOMPARE(controller.ambientLevel(),12);
        controller.setNoiseControlOff();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),5000);
        QVERIFY2(controller.lastError().isEmpty(),qPrintable(controller.lastError()));
        QCOMPARE(controller.noiseControlMode(),QString("off"));
        QCOMPARE(controller.ambientLevel(),12);
    }
    void twsBatteryReadingsAreExposedAndCleared() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"), "WF-1000XM4");
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QCOMPARE(controller.batteryLeft(), 81);
        QCOMPARE(controller.batteryRight(), 79);
        QCOMPARE(controller.batteryCase(), 85);
        controller.disconnectDevice();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QCOMPARE(controller.batteryLeft(), -1);
        QCOMPARE(controller.batteryRight(), -1);
        QCOMPARE(controller.batteryCase(), -1);
    }
    void destructionCancelsPendingBackendWork() {
        auto service = std::make_shared<CancellableService>();
        auto controller = std::make_unique<DeviceCenterController>(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(service->entered.load(), 1500);
        QElapsedTimer elapsed; elapsed.start();
        controller.reset();
        QVERIFY(elapsed.elapsed() < 500);
    }
    void startupCompletesBeforeBluetoothMaintenance() {
        auto service = std::make_shared<SlowService>();
        DeviceBackend backend(service);
        QSignalSpy completed(&backend, &DeviceBackend::completed);
        backend.start();
        QCOMPARE(completed.count(), 1);
        QCOMPARE(service->ticks.load(), 0);
        backend.shutdown();
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
    void equalizerCapabilities_data() {
        QTest::addColumn<QString>("model");
        QTest::addColumn<bool>("tenBand");
        QTest::newRow("WH-1000XM6") << QString("WH-1000XM6") << true;
        QTest::newRow("WH-1000XM5") << QString("WH-1000XM5") << false;
    }
    void equalizerCapabilities() {
        QFETCH(QString, model);
        QFETCH(bool, tenBand);
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"), model.toStdString());
        DeviceCenterController controller(nullptr, service);
        QSignalSpy capabilitiesChanged(&controller, &DeviceCenterController::capabilitiesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QVERIFY(!capabilitiesChanged.isEmpty());
        QVERIFY(controller.hasEqualizer());
        QCOMPARE(controller.tenBandEqualizer(), tenBand);
        QCOMPARE(controller.property("tenBandEqualizer"), QVariant(tenBand));
        QCOMPARE(controller.hasClearBass(), !tenBand);
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
    void ambientWithUnknownLevelUsesFallback() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"),"WH-1000XM6");
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        QCOMPARE(controller.ambientLevel(),10);
        auto lastAmbientLevel = [&transport]() -> int {
            int level = -1;
            for (const auto& bytes : transport->sentFrames()) {
                const auto p = protocol::FrameCodec::decode(bytes).payload;
                if (p.size() == 7 && p[0] == 0x68 && p[1] == 0x17) level = p[6];
            }
            return level;
        };
        controller.setAmbient(0, false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        QVERIFY2(controller.lastError().isEmpty(), qPrintable(controller.lastError()));
        QCOMPARE(lastAmbientLevel(),10);
        controller.setAmbient(15, false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        controller.setAmbient(0, true);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        QVERIFY2(controller.lastError().isEmpty(), qPrintable(controller.lastError()));
        QCOMPARE(lastAmbientLevel(),15);
    }
    void destructionDrainsWorkerAndCallbacks() {
        auto service = std::make_shared<SlowService>();
        auto controller = std::make_unique<DeviceCenterController>(nullptr,service);
        QTest::qWait(20);
        controller.reset();
        QCoreApplication::processEvents();
    }
};
QTEST_GUILESS_MAIN(DeviceControllerTests)
#include "DeviceControllerTests.moc"
