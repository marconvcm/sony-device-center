#include <QtTest>
#include <QScopeGuard>
#include "DeviceCenterController.h"
#include "sony/core/DeviceService.h"
#include "sony/core/ControlSettings.h"
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
        QCOMPARE(controller.firmware(), QString("Unknown"));
    }
    void metadataDisplaysOnlyWhileValidAndConnected() {
        class MetadataService : public SlowService {
        public:
            std::atomic<bool> connected{true}, stale{false};
            void tick() override {}
            bool isConnected() const noexcept override { return connected; }
            protocol::DeviceStateSnapshot snapshot() const override {
                auto state = std::make_shared<protocol::DeviceState>();
                state->battery.main = 12;
                state->codec = "LC3";
                state->firmware = "3.1.5";
                for (const auto* feature : {"battery", "codec", "firmware"})
                    state->features[feature].availability = stale ? "stale" : "valid";
                return state;
            }
        };
        auto service = std::make_shared<MetadataService>();
        DeviceCenterController controller(nullptr, service);
        QTRY_COMPARE(controller.batteryLevel(), 12);
        QCOMPARE(controller.codec(), QString("LC3"));
        QCOMPARE(controller.firmware(), QString("3.1.5"));
        service->stale = true;
        QTRY_COMPARE(controller.batteryLevel(), -1);
        QCOMPARE(controller.codec(), QString("Unknown"));
        QCOMPARE(controller.firmware(), QString("Unknown"));
        service->stale = false;
        QTRY_COMPARE(controller.firmware(), QString("3.1.5"));
        service->connected = false;
        QTRY_COMPARE(controller.batteryLevel(), -1);
        QCOMPARE(controller.codec(), QString("Unknown"));
        QCOMPARE(controller.firmware(), QString("Unknown"));
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
    void destructionDrainsWorkerAndCallbacks() {
        auto service = std::make_shared<SlowService>();
        auto controller = std::make_unique<DeviceCenterController>(nullptr,service);
        QTest::qWait(20);
        controller.reset();
        QCoreApplication::processEvents();
    }
    void ambientLevelSurvivesAncAndOff() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        transport->notify({0x69,0x17,1,1,1,0,12});
        QTRY_COMPARE(controller.ambientLevel(),12);
        transport->notify({0x69,0x17,1,1,0,0,0});
        QTRY_COMPARE(controller.noiseControlMode(),QString("cancelling"));
        QCOMPARE(controller.ambientLevel(),12);
        transport->notify({0x69,0x17,1,0,0,0,0});
        QTRY_COMPARE(controller.noiseControlMode(),QString("off"));
        QCOMPARE(controller.ambientLevel(),12);
    }
};
QTEST_GUILESS_MAIN(DeviceControllerTests)
#include "DeviceControllerTests.moc"
