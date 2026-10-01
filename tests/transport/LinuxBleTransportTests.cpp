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
    }
    SECTION("BLE") {
        transport.connect("11:22:33:44:55:66");
        REQUIRE_FALSE(raw->isConnected());
        REQUIRE(transport.controlBearer() == ControlBearer::BleGatt);
        REQUIRE(gatt->acquired == std::vector<bool>{true, false});
        transport.disconnect(); transport.disconnect();
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
        transport.cancelPending();
        REQUIRE(reader.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready);
        REQUIRE(reader.get() == SonyErrorCode::Disconnected);
        REQUIRE(gatt->rxPeer >= 0); // Cancellation leaves cleanup to the owner.
        transport.disconnect();
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
TEST_CASE("Linux BLE transport forwards cancellation to Classic", "[transport][ble]") {
    class CancellableClassic : public FakeTransport {
    public:
        int cancellations{0};
        void cancelPending() noexcept override { ++cancellations; }
    };
    auto classic = std::make_unique<CancellableClassic>();
    auto* raw = classic.get();
    auto client = std::make_unique<GattClient>();
    client->useBle = false;
    LinuxBleTransport transport(std::move(classic), std::move(client));
    transport.connect("11:22:33:44:55:66");
    const int previous = raw->cancellations;
    transport.cancelPending();
    REQUIRE(raw->cancellations == previous + 1);
    REQUIRE(raw->isConnected());
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
    SECTION("Complete endpoints can be acquired when the aggregate discovery flag is false") {
        device.booleans["org.bluez.Device1/ServicesResolved"] = false;
        const auto endpoint = selectGattEndpoint(objects, address);
        REQUIRE(endpoint);
        REQUIRE(endpoint->writePath == "/write");
        REQUIRE(endpoint->notifyPath == "/notify");
    }
    SECTION("Incomplete discovery still waits for missing endpoints") {
        device.booleans["org.bluez.Device1/ServicesResolved"] = false;
        objects.erase("/notify");
        REQUIRE_THROWS_AS(selectGattEndpoint(objects, address), SonyException);
    }
    SECTION("Cached endpoints cannot override a disconnected device") {
        device.booleans["org.bluez.Device1/Connected"] = false;
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
#endif
