#include <QtTest>
#include "DeviceCenterController.h"
#include "sony/core/DeviceService.h"
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
    QTemporaryDir settingsDirectory;
private slots:
    void initTestCase() {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    }
    void init() { QSettings(QSettings::defaultFormat(), QSettings::UserScope, "SonyBridge", "SonyDeviceCenter").clear(); }
    void remembersOnlySuccessfulConnections() {
        QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "SonyBridge", "SonyDeviceCenter");
        const QString remembered("33:22:33:44:55:66");
        settings.setValue("lastConnectedAddress", remembered);
        auto transport = std::make_shared<ReplyTransport>();
        transport->failAddress = "22:22:33:44:55:66";
        auto discovery = std::make_shared<sony::transport::FakeDeviceDiscovery>();
        discovery->addDevice({"WH-1000XM5",sony::transport::DeviceAddress(transport->failAddress),true,true});
        auto service = std::make_shared<core::DeviceService>(transport, discovery);
        DeviceCenterController controller(nullptr, service);
        QTRY_COMPARE_WITH_TIMEOUT(controller.connectionState(), QString("selection_required"), 4000);
        QCOMPARE(settings.value("lastConnectedAddress").toString(), remembered);
        transport->failAddress.clear();
        controller.connectDevice("22:22:33:44:55:66", "WH-1000XM5");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && controller.isConnected(), 3000);
        QCOMPARE(settings.value("lastConnectedAddress").toString(), QString("22:22:33:44:55:66"));
    }
    void startupReusesSavedHeadsetWhenOffline() {
        QSettings(QSettings::defaultFormat(), QSettings::UserScope, "SonyBridge", "SonyDeviceCenter").setValue("lastConnectedAddress", "33:22:33:44:55:66");
        auto transport = std::make_shared<ReplyTransport>();
        auto discovery = std::make_shared<sony::transport::FakeDeviceDiscovery>();
        discovery->addDevice({"WH-1000XM5",sony::transport::DeviceAddress("33:22:33:44:55:66"),true,false});
        auto service = std::make_shared<core::DeviceService>(transport, discovery);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(controller.isConnected(), 3000);
        QCOMPARE(controller.deviceAddress(), QString("33:22:33:44:55:66"));
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
    void ambientUsesV1MaximumWhenDeviceReportsLevel20() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"),"WH-1000XM4");
        transport->queueIncoming(sony::protocol::FrameCodec::encode({sony::protocol::DataType::Ack, 0, {}}));
        service->activeDevice()->setAmbient(20, false);

        DeviceCenterController controller(nullptr, service);
        QTRY_COMPARE_WITH_TIMEOUT(controller.ambientLevel(), 20, 2000);
        const auto sentBefore = transport->sentCount();
        controller.setAmbient(controller.ambientLevel(), false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QCOMPARE(controller.lastError(), QString());
        QCOMPARE(transport->sentCount(), sentBefore + 1);
        const auto frame = sony::protocol::FrameCodec::decode(transport->lastSentFrame());
        QCOMPARE(frame.payload.back(), static_cast<uint8_t>(19));
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
