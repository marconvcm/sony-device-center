#include <catch2/catch_test_macros.hpp>
#include "sony/transport/FakeTransport.h"
#include "sony/transport/SonyError.h"

using namespace sony::transport;
using namespace sony;

namespace {

template <typename F>
void requireErrorCode(F&& fn, SonyErrorCode expectedCode) {
    try {
        fn();
        FAIL("Expected SonyException not thrown");
    } catch (const SonyException& e) {
        REQUIRE(e.code() == expectedCode);
    }
}

} // namespace

TEST_CASE("FakeTransport handles connection lifecycle", "[transport][fake]")
{
    FakeTransport transport;

    REQUIRE_FALSE(transport.isConnected());

    DeviceAddress addr("11:22:33:44:55:66");
    transport.connect(addr);

    REQUIRE(transport.isConnected());
    REQUIRE(transport.connectedAddress() == addr);

    transport.disconnect();
    REQUIRE_FALSE(transport.isConnected());
}

TEST_CASE("FakeTransport fails connection when configured", "[transport][fake]")
{
    FakeTransport transport;
    transport.setFailConnect(true, SonyErrorCode::TransportFailure);

    requireErrorCode([&]() {
        transport.connect(DeviceAddress("AA:BB:CC:DD:EE:FF"));
    }, SonyErrorCode::TransportFailure);

    REQUIRE_FALSE(transport.isConnected());
}

TEST_CASE("FakeTransport send records frames", "[transport][fake]")
{
    FakeTransport transport;
    transport.connect(DeviceAddress("11:22:33:44:55:66"));

    std::vector<uint8_t> frame1 = {0x0c, 0x00, 0x01, 0x02};
    std::vector<uint8_t> frame2 = {0x0c, 0x00, 0x03, 0x04};

    std::vector<std::byte> byteSpan1;
    for (auto b : frame1) byteSpan1.push_back(static_cast<std::byte>(b));

    std::vector<std::byte> byteSpan2;
    for (auto b : frame2) byteSpan2.push_back(static_cast<std::byte>(b));

    size_t sent1 = transport.send(byteSpan1);
    size_t sent2 = transport.send(byteSpan2);

    REQUIRE(sent1 == 4);
    REQUIRE(sent2 == 4);
    REQUIRE(transport.sentCount() == 2);

    std::vector<std::vector<uint8_t>> expected = {frame1, frame2};
    REQUIRE(transport.sentFrames() == expected);
    REQUIRE(transport.lastSentFrame() == frame2);

    std::vector<uint8_t> expectedAll = {0x0c, 0x00, 0x01, 0x02, 0x0c, 0x00, 0x03, 0x04};
    REQUIRE(transport.allSentBytes() == expectedAll);

    transport.clearSent();
    REQUIRE(transport.sentCount() == 0);
    REQUIRE(transport.sentFrames().empty());
}

TEST_CASE("FakeTransport queue incoming and receive delivers data", "[transport][fake]")
{
    FakeTransport transport;
    transport.connect(DeviceAddress("11:22:33:44:55:66"));

    std::vector<uint8_t> payload = {0x0c, 0x01, 0x02, 0x03, 0x04};
    transport.queueIncoming(payload);

    REQUIRE(transport.incomingBytesAvailable() == 5);

    std::array<std::byte, 10> buf{};
    size_t read = transport.receive(buf);

    REQUIRE(read == 5);
    for (size_t i = 0; i < read; ++i) {
        REQUIRE(static_cast<uint8_t>(buf[i]) == payload[i]);
    }
    REQUIRE(transport.incomingBytesAvailable() == 0);
}

TEST_CASE("FakeTransport delivers multiple frames in one read if buffer allows", "[transport][fake]")
{
    FakeTransport transport;
    transport.connect(DeviceAddress("11:22:33:44:55:66"));

    std::vector<uint8_t> frame1 = {0x3e, 0x0c, 0x01, 0x3c};
    std::vector<uint8_t> frame2 = {0x3e, 0x0c, 0x02, 0x3c};
    transport.queueIncoming({frame1, frame2});

    REQUIRE(transport.incomingBytesAvailable() == 8);

    std::array<std::byte, 16> buf{};
    size_t read = transport.receive(buf);

    REQUIRE(read == 8);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE(static_cast<uint8_t>(buf[i]) == frame1[i]);
    }
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE(static_cast<uint8_t>(buf[4 + i]) == frame2[i]);
    }
}

TEST_CASE("FakeTransport simulates fragmented messages via maxReceiveChunkSize", "[transport][fake]")
{
    FakeTransport transport;
    transport.connect(DeviceAddress("11:22:33:44:55:66"));

    std::vector<uint8_t> frame = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    transport.queueIncoming(frame);
    transport.setMaxReceiveChunkSize(2); // return at most 2 bytes per read

    std::array<std::byte, 10> buf{};

    // Read 1: 2 bytes
    size_t r1 = transport.receive(buf);
    REQUIRE(r1 == 2);
    REQUIRE(static_cast<uint8_t>(buf[0]) == 0x01);
    REQUIRE(static_cast<uint8_t>(buf[1]) == 0x02);

    // Read 2: 2 bytes
    size_t r2 = transport.receive(buf);
    REQUIRE(r2 == 2);
    REQUIRE(static_cast<uint8_t>(buf[0]) == 0x03);
    REQUIRE(static_cast<uint8_t>(buf[1]) == 0x04);

    // Read 3: 2 bytes
    size_t r3 = transport.receive(buf);
    REQUIRE(r3 == 2);
    REQUIRE(static_cast<uint8_t>(buf[0]) == 0x05);
    REQUIRE(static_cast<uint8_t>(buf[1]) == 0x06);

    // Read 4: 1 byte remaining
    size_t r4 = transport.receive(buf);
    REQUIRE(r4 == 1);
    REQUIRE(static_cast<uint8_t>(buf[0]) == 0x07);

    REQUIRE(transport.incomingBytesAvailable() == 0);
}

TEST_CASE("FakeTransport simulates timeouts on receive and send", "[transport][fake]")
{
    FakeTransport transport;
    transport.connect(DeviceAddress("11:22:33:44:55:66"));

    SECTION("Timeout on empty queue") {
        std::array<std::byte, 10> buf{};
        requireErrorCode([&]() {
            transport.receive(buf);
        }, SonyErrorCode::Timeout);
    }

    SECTION("Explicit timeout on receive") {
        transport.queueIncoming({0x01, 0x02});
        transport.simulateTimeoutOnReceive(true, 1);

        std::array<std::byte, 10> buf{};
        requireErrorCode([&]() {
            transport.receive(buf);
        }, SonyErrorCode::Timeout);

        // Next read succeeds as count was 1
        size_t r = transport.receive(buf);
        REQUIRE(r == 2);
    }

    SECTION("Explicit timeout on send") {
        transport.simulateTimeoutOnSend(true, 1);
        std::array<std::byte, 2> data{std::byte{0x01}, std::byte{0x02}};

        requireErrorCode([&]() {
            transport.send(data);
        }, SonyErrorCode::Timeout);

        // Next send succeeds as count was 1
        size_t sent = transport.send(data);
        REQUIRE(sent == 2);
    }
}

TEST_CASE("FakeTransport simulates disconnects", "[transport][fake]")
{
    FakeTransport transport;
    transport.connect(DeviceAddress("11:22:33:44:55:66"));
    REQUIRE(transport.isConnected());

    transport.simulateDisconnect();
    REQUIRE_FALSE(transport.isConnected());

    std::array<std::byte, 2> data{std::byte{0x01}, std::byte{0x02}};
    requireErrorCode([&]() {
        transport.send(data);
    }, SonyErrorCode::Disconnected);

    std::array<std::byte, 10> buf{};
    requireErrorCode([&]() {
        transport.receive(buf);
    }, SonyErrorCode::Disconnected);
}

TEST_CASE("FakeDeviceDiscovery records and returns devices", "[transport][fake]")
{
    FakeDeviceDiscovery discovery;
    REQUIRE(discovery.discover().empty());

    DiscoveredDevice dev1{.name = "Sony WH-1000XM4", .address = DeviceAddress("AA:BB:CC:DD:EE:01")};
    DiscoveredDevice dev2{.name = "Sony WH-1000XM5", .address = DeviceAddress("AA:BB:CC:DD:EE:02")};

    discovery.addDevice(dev1);
    discovery.addDevice(dev2);

    auto list = discovery.discover();
    REQUIRE(list.size() == 2);
    REQUIRE(list[0] == dev1);
    REQUIRE(list[1] == dev2);

    discovery.clear();
    REQUIRE(discovery.discover().empty());
}
