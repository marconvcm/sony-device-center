#include <catch2/catch_test_macros.hpp>
#if defined(SONY_HAS_LINUX_BLUETOOTH)
#include "sony/transport/LinuxBleTransport.h"
#include "sony/transport/PlatformTransport.h"
#include "../../libs/sony-transport/src/BluezGattObjects.h"
#include "sony/transport/FakeTransport.h"
#include "sony/transport/SonyError.h"
#include <array>
#include <future>
#include <sys/socket.h>
#include <unistd.h>

using namespace sony;
using namespace sony::transport;
namespace {
class GattClient : public IBluezGattClient {
public:
    bool useBle{true}, failWrite{false};
    int rxPeer{-1}, txPeer{-1};
    std::vector<bool> acquired;
    DeviceMetadata deviceMetadata() override { return {42, "LC3", "3.1.5"}; }
    std::optional<GattEndpoint> resolve(const DeviceAddress&) override {
        if (!useBle) return std::nullopt;
        return GattEndpoint{"write", "notify"};
    }
    GattDescriptor acquire(const std::string&, bool notify) override {
        acquired.push_back(notify);
        if (!notify && failWrite) throw SonyException(SonyErrorCode::TransportFailure, "acquire failed");
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds)) throw std::runtime_error("socketpair");
        (notify ? rxPeer : txPeer) = fds[1];
        return {fds[0], 23};
    }
    void release() noexcept override {
        if (rxPeer >= 0) close(rxPeer);
        if (txPeer >= 0) close(txPeer);
        rxPeer = txPeer = -1;
    }
    ~GattClient() override { release(); }
};
}

TEST_CASE("Linux BLE transport routes Classic and acquires notification before writes", "[transport][ble]") {
    auto classic = std::make_unique<FakeTransport>(); auto* raw = classic.get();
    auto client = std::make_unique<GattClient>(); auto* gatt = client.get();
    LinuxBleTransport transport(std::move(classic), std::move(client));
    SECTION("Classic fallback") {
        gatt->useBle = false;
        transport.connect("11:22:33:44:55:66");
        REQUIRE(raw->isConnected());
        REQUIRE(transport.controlBearer() == ControlBearer::Rfcomm);
        REQUIRE(gatt->acquired.empty());
        REQUIRE_FALSE(transport.deviceMetadata().batteryPercentage);
    }
    SECTION("BLE") {
        transport.connect("11:22:33:44:55:66");
        REQUIRE_FALSE(raw->isConnected());
        REQUIRE(transport.controlBearer() == ControlBearer::BleGatt);
        REQUIRE(gatt->acquired == std::vector<bool>{true, false});
        REQUIRE(transport.deviceMetadata().batteryPercentage == 42);
        transport.disconnect(); transport.disconnect();
        REQUIRE_FALSE(transport.deviceMetadata().batteryPercentage);
        REQUIRE_FALSE(transport.isConnected());
        transport.connect("11:22:33:44:55:66");
        REQUIRE(transport.isConnected());
    }
    SECTION("Acquisition failure does not open RFCOMM") {
        gatt->failWrite = true;
        REQUIRE_THROWS(transport.connect("11:22:33:44:55:66"));
        REQUIRE_FALSE(raw->isConnected());
        REQUIRE_FALSE(transport.isConnected());
        REQUIRE(gatt->rxPeer == -1);
    }
}

TEST_CASE("Linux BLE transport retains notification tails and respects ATT MTU", "[transport][ble]") {
    auto client = std::make_unique<GattClient>(); auto* gatt = client.get();
    LinuxBleTransport transport(std::make_unique<FakeTransport>(), std::move(client));
    transport.connect("11:22:33:44:55:66");
    std::array<std::byte, 32> data{};
    REQUIRE(transport.send(data) == 20);
    REQUIRE(read(gatt->txPeer, data.data(), data.size()) == 20);
    const std::array<uint8_t, 5> notification{1,2,3,4,5};
    REQUIRE(write(gatt->rxPeer, notification.data(), notification.size()) == 5);
    std::array<std::byte, 3> received;
    REQUIRE(transport.receive(received) == 3);
    REQUIRE(received[2] == std::byte{3});
    REQUIRE(transport.receive(received) == 2);
    REQUIRE(received[0] == std::byte{4});
    REQUIRE(received[1] == std::byte{5});
}

TEST_CASE("Linux BLE transport wakes blocked readers on cancellation and peer closure", "[transport][ble]") {
    auto client = std::make_unique<GattClient>(); auto* gatt = client.get();
    LinuxBleTransport transport(std::make_unique<FakeTransport>(), std::move(client));
    transport.connect("11:22:33:44:55:66");
    SECTION("Cancellation") {
        auto reader = std::async(std::launch::async, [&] {
            std::array<std::byte, 10> bytes;
            try { transport.receive(bytes); } catch (const SonyException& e) { return e.code(); }
            return SonyErrorCode::InvalidResponse;
        });
        REQUIRE(reader.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout);
        transport.disconnect();
        REQUIRE(reader.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready);
        REQUIRE(reader.get() == SonyErrorCode::Disconnected);
    }
    SECTION("Peer closure") {
        close(gatt->rxPeer); gatt->rxPeer = -1;
        std::array<std::byte, 10> bytes;
        REQUIRE_THROWS(transport.receive(bytes));
        REQUIRE_FALSE(transport.isConnected());
    }
    SECTION("Write peer closure") {
        close(gatt->txPeer); gatt->txPeer = -1;
        const std::array<std::byte, 1> bytes{std::byte{0x3e}};
        REQUIRE_THROWS(transport.send(bytes));
        REQUIRE_FALSE(transport.isConnected());
    }
    SECTION("An idle notification timeout keeps the connection") {
        std::array<std::byte, 10> bytes;
        try { transport.receive(bytes); FAIL("Expected timeout"); }
        catch (const SonyException& e) { REQUIRE(e.code() == SonyErrorCode::Timeout); }
        REQUIRE(transport.isConnected());
    }
}
TEST_CASE("Linux BLE control is disabled unless explicitly enabled", "[transport][ble]") {
    auto defaultTransport = createPlatformTransport();
    auto disabled = createPlatformTransport(false);
    auto enabled = createPlatformTransport(true);
    REQUIRE(dynamic_cast<LinuxBleTransport*>(defaultTransport.get()) == nullptr);
    REQUIRE(dynamic_cast<LinuxBleTransport*>(disabled.get()) == nullptr);
    REQUIRE(dynamic_cast<LinuxBleTransport*>(enabled.get()) != nullptr);
}

TEST_CASE("BlueZ routing selects only a ready XM6 LE control service", "[transport][ble]") {
    using namespace sony::transport::detail;
    std::map<std::string, BluezObject> objects;
    auto& device = objects["/device"];
    device.strings = {{"org.bluez.Device1/Address", "AA:BB:CC:DD:EE:FF"},
                      {"org.bluez.Device1/Name", "WH-1000XM6"},
                      {"org.bluez.Device1/PreferredBearer", "le"}};
    device.booleans = {{"org.bluez.Device1/Connected", true},
                       {"org.bluez.Device1/Paired", true},
                       {"org.bluez.Device1/ServicesResolved", true},
                       {"org.bluez.Bearer.LE1/Connected", true}};
    objects["/service"].strings = {{"org.bluez.GattService1/Device", "/device"},
                                   {"org.bluez.GattService1/UUID", SonyControlService}};
    objects["/write"].strings = {{"org.bluez.GattCharacteristic1/Service", "/service"},
                                 {"org.bluez.GattCharacteristic1/UUID", SonyControlWrite}};
    objects["/write"].arrays["org.bluez.GattCharacteristic1/Flags"] = {"write-without-response"};
    objects["/notify"].strings = {{"org.bluez.GattCharacteristic1/Service", "/service"},
                                  {"org.bluez.GattCharacteristic1/UUID", SonyControlNotify}};
    objects["/notify"].arrays["org.bluez.GattCharacteristic1/Flags"] = {"notify"};
    const DeviceAddress address("aa:bb:cc:dd:ee:ff");

    SECTION("Live LE bearer takes precedence over connection preference") {
        device.strings["org.bluez.Device1/PreferredBearer"] = "last-used";
        auto endpoint = selectGattEndpoint(objects, address);
        REQUIRE(endpoint);
        REQUIRE(endpoint->writePath == "/write");
        REQUIRE(endpoint->notifyPath == "/notify");
    }
    SECTION("Older BlueZ can use the explicit LE preference") {
        device.booleans.erase("org.bluez.Bearer.LE1/Connected");
        REQUIRE(selectGattEndpoint(objects, address));
    }
    SECTION("Cached services do not select BLE on a Classic connection") {
        device.strings["org.bluez.Device1/PreferredBearer"] = "bredr";
        device.booleans["org.bluez.Bearer.LE1/Connected"] = false;
        REQUIRE_FALSE(selectGattEndpoint(objects, address));
        device.booleans.erase("org.bluez.Bearer.LE1/Connected");
        REQUIRE_FALSE(selectGattEndpoint(objects, address));
    }
    SECTION("Unknown models stay on the existing path") {
        device.strings["org.bluez.Device1/Name"] = "Other device";
        REQUIRE_FALSE(selectGattEndpoint(objects, address));
    }
    SECTION("Unpaired devices stay on the existing path") {
        device.booleans["org.bluez.Device1/Paired"] = false;
        REQUIRE_FALSE(selectGattEndpoint(objects, address));
    }
    SECTION("An explicitly disconnected LE bearer cannot fall back to Classic") {
        device.booleans["org.bluez.Bearer.LE1/Connected"] = false;
        REQUIRE_THROWS_AS(selectGattEndpoint(objects, address), SonyException);
    }
    SECTION("Service discovery must finish") {
        device.booleans["org.bluez.Device1/ServicesResolved"] = false;
        REQUIRE_THROWS_AS(selectGattEndpoint(objects, address), SonyException);
    }
    SECTION("Missing endpoints cannot fall back to Classic") {
        objects.erase("/notify");
        REQUIRE_THROWS_AS(selectGattEndpoint(objects, address), SonyException);
    }
    SECTION("Characteristic flags must support acquired writes") {
        objects["/write"].arrays["org.bluez.GattCharacteristic1/Flags"] = {"write"};
        REQUIRE_THROWS_AS(selectGattEndpoint(objects, address), SonyException);
    }
    SECTION("Characteristics must belong to this device's service") {
        objects["/notify"].strings["org.bluez.GattCharacteristic1/Service"] = "/other-service";
        REQUIRE_THROWS_AS(selectGattEndpoint(objects, address), SonyException);
    }
}
TEST_CASE("BlueZ metadata belongs to the connected device and active BAP transport", "[transport][ble]") {
    using namespace sony::transport::detail;
    std::map<std::string, BluezObject> objects;
    auto& device = objects["/device"];
    device.booleans = {{"org.bluez.Device1/Connected", true}, {"org.bluez.Device1/ServicesResolved", true}};
    device.bytes["org.bluez.Battery1/Percentage"] = 12;
    auto& audio = objects["/audio"];
    audio.strings = {{"org.bluez.MediaTransport1/Device", "/device"},
                     {"org.bluez.MediaTransport1/UUID", "00002bcb-0000-1000-8000-00805f9b34fb"},
                     {"org.bluez.MediaTransport1/State", "active"}};
    audio.bytes["org.bluez.MediaTransport1/Codec"] = 6;
    auto& service = objects["/info"];
    service.strings = {{"org.bluez.GattService1/Device", "/device"},
                       {"org.bluez.GattService1/UUID", "0000180a-0000-1000-8000-00805f9b34fb"}};
    auto& firmware = objects["/version"];
    firmware.strings = {{"org.bluez.GattCharacteristic1/Service", "/info"},
                        {"org.bluez.GattCharacteristic1/UUID", "00002a26-0000-1000-8000-00805f9b34fb"}};
    firmware.arrays["org.bluez.GattCharacteristic1/Flags"] = {"read"};
    firmware.strings["org.bluez.GattCharacteristic1/Value"] = "3.1.5";
    SECTION("Standard metadata") {
        const auto metadata = readDeviceMetadata(objects, "/device");
        REQUIRE(metadata.batteryPercentage == 12);
        REQUIRE(metadata.codec == "LC3");
        REQUIRE(metadata.firmware == "3.1.5");
    }
    SECTION("No cached readings after disconnect or removal") {
        device.booleans["org.bluez.Device1/Connected"] = false;
        REQUIRE_FALSE(readDeviceMetadata(objects, "/device").batteryPercentage);
        REQUIRE(readDeviceMetadata(objects, "/device").codec.empty());
        REQUIRE(readDeviceMetadata(objects, "/device").firmware.empty());
        REQUIRE_FALSE(readDeviceMetadata(objects, "/missing").batteryPercentage);
    }
    SECTION("Battery zero is valid, absent or invalid values are unknown") {
        device.bytes["org.bluez.Battery1/Percentage"] = 0;
        REQUIRE(readDeviceMetadata(objects, "/device").batteryPercentage == 0);
        device.bytes["org.bluez.Battery1/Percentage"] = 255;
        REQUIRE_FALSE(readDeviceMetadata(objects, "/device").batteryPercentage);
        device.bytes.clear();
        REQUIRE_FALSE(readDeviceMetadata(objects, "/device").batteryPercentage);
    }
    SECTION("Idle transport does not imply active LC3") {
        audio.strings["org.bluez.MediaTransport1/State"] = "idle";
        REQUIRE(readDeviceMetadata(objects, "/device").codec.empty());
    }
    SECTION("Codec 6 on A2DP is not LC3") {
        audio.strings["org.bluez.MediaTransport1/UUID"] = "0000110b-0000-1000-8000-00805f9b34fb";
        REQUIRE(readDeviceMetadata(objects, "/device").codec.empty());
    }
    SECTION("BAP source is recognized too") {
        audio.strings["org.bluez.MediaTransport1/UUID"] = "00002bc9-0000-1000-8000-00805f9b34fb";
        REQUIRE(readDeviceMetadata(objects, "/device").codec == "LC3");
        audio.bytes["org.bluez.MediaTransport1/Codec"] = 255;
        REQUIRE(readDeviceMetadata(objects, "/device").codec.empty());
    }
    SECTION("Other devices cannot supply firmware or codec") {
        audio.strings["org.bluez.MediaTransport1/Device"] = "/other";
        service.strings["org.bluez.GattService1/Device"] = "/other";
        REQUIRE(readDeviceMetadata(objects, "/device").codec.empty());
        REQUIRE(readDeviceMetadata(objects, "/device").firmware.empty());
    }
    SECTION("Uncached firmware can be read from the identified characteristic") {
        firmware.strings.erase("org.bluez.GattCharacteristic1/Value");
        REQUIRE(readDeviceMetadata(objects, "/device").firmware.empty());
        REQUIRE(firmwareCharacteristic(objects, "/device") == "/version");
        REQUIRE(firmwareString({}).empty());
        REQUIRE(firmwareString({'3', 0, '5'}).empty());
        REQUIRE(firmwareString(std::string(65, 'a')).empty());
    }
}
#endif
