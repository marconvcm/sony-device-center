#include <catch2/catch_test_macros.hpp>
#include "sony/transport/FakeTransport.h"
#include "sony/transport/BluetoothConnectorTransport.h"
#include "sony/transport/TransportBluetoothConnector.h"
#include "sony/transport/LinuxBluetoothTransport.h"
#include "sony/transport/WindowsBluetoothTransport.h"
#include "sony/transport/MacOSBluetoothTransport.h"
#include "BluetoothWrapper.h"
#include "CommandSerializer.h"

#include <algorithm>
#include <array>

using namespace sony::transport;
using namespace sony;

namespace {

class MockConnector final : public IBluetoothConnector {
public:
    int send(char* buf, size_t length) override {
        if (!_connected) throw RecoverableException("Not connected", true);
        sent.assign(buf, buf + length);
        return static_cast<int>(length);
    }

    int recv(char* buf, size_t length) override {
        if (!_connected) throw RecoverableException("Not connected", true);
        if (incoming.empty()) throw RecoverableException("No data", false);
        size_t toCopy = std::min(length, incoming.size());
        std::copy_n(incoming.data(), toCopy, buf);
        incoming.erase(incoming.begin(), incoming.begin() + toCopy);
        return static_cast<int>(toCopy);
    }

    void connect(const std::string& addrStr) override {
        connectedAddr = addrStr;
        _connected = true;
    }

    void disconnect() noexcept override {
        _connected = false;
    }

    bool isConnected() noexcept override {
        return _connected;
    }

    std::vector<BluetoothDevice> getConnectedDevices() override {
        return devices;
    }

    SonyProtocolVersion getProtocolVersion() noexcept override {
        return version;
    }

    bool _connected{false};
    std::string connectedAddr;
    std::vector<char> sent;
    std::vector<char> incoming;
    std::vector<BluetoothDevice> devices;
    SonyProtocolVersion version{SonyProtocolVersion::V2};
};

} // namespace

TEST_CASE("BluetoothConnectorTransport wraps an IBluetoothConnector as ITransport", "[transport][adapter]")
{
    auto mock = std::make_unique<MockConnector>();
    MockConnector* rawMock = mock.get();

    BluetoothConnectorTransport transport(std::move(mock));
    REQUIRE_FALSE(transport.isConnected());

    transport.connect(DeviceAddress("AA:BB:CC:DD:EE:FF"));
    REQUIRE(transport.isConnected());
    REQUIRE(rawMock->connectedAddr == "AA:BB:CC:DD:EE:FF");

    // Test send
    std::vector<std::byte> out = {std::byte{0x3e}, std::byte{0x0c}, std::byte{0x3c}};
    size_t sent = transport.send(out);
    REQUIRE(sent == 3);
    REQUIRE(rawMock->sent.size() == 3);
    REQUIRE(static_cast<uint8_t>(rawMock->sent[0]) == 0x3e);

    // Test receive
    rawMock->incoming = {'H', 'e', 'l', 'l', 'o'};
    std::array<std::byte, 10> inBuf{};
    size_t recvd = transport.receive(inBuf);
    REQUIRE(recvd == 5);
    REQUIRE(static_cast<char>(inBuf[0]) == 'H');

    // Test discovery adapter
    rawMock->devices = {{"WH-1000XM4", "11:22:33:44:55:66"}};
    BluetoothConnectorDiscovery discovery(rawMock);
    auto discovered = discovery.discover();
    REQUIRE(discovered.size() == 1);
    REQUIRE(discovered[0].name == "WH-1000XM4");
    REQUIRE(discovered[0].address.str() == "11:22:33:44:55:66");

    transport.disconnect();
    REQUIRE_FALSE(transport.isConnected());
}

TEST_CASE("TransportBluetoothConnector adapts ITransport into IBluetoothConnector", "[transport][adapter]")
{
    auto fakeTransport = std::make_unique<FakeTransport>();
    FakeTransport* rawFake = fakeTransport.get();

    auto fakeDiscovery = std::make_unique<FakeDeviceDiscovery>();
    fakeDiscovery->addDevice(DiscoveredDevice{.name = "Sony XM5", .address = DeviceAddress("55:44:33:22:11:00")});

    TransportBluetoothConnector connector(std::move(fakeTransport), std::move(fakeDiscovery), SonyProtocolVersion::V2);

    REQUIRE_FALSE(connector.isConnected());
    connector.connect("55:44:33:22:11:00");
    REQUIRE(connector.isConnected());
    REQUIRE(rawFake->connectedAddress() == DeviceAddress("55:44:33:22:11:00"));

    // Send through connector
    char sendBuf[] = {0x0c, 0x01, 0x02};
    int sent = connector.send(sendBuf, 3);
    REQUIRE(sent == 3);
    REQUIRE(rawFake->sentCount() == 1);
    REQUIRE(rawFake->lastSentFrame() == std::vector<uint8_t>{0x0c, 0x01, 0x02});

    // Recv through connector
    rawFake->queueIncoming({0xaa, 0xbb});
    char recvBuf[10]{};
    int recvd = connector.recv(recvBuf, 10);
    REQUIRE(recvd == 2);
    REQUIRE(static_cast<uint8_t>(recvBuf[0]) == 0xaa);
    REQUIRE(static_cast<uint8_t>(recvBuf[1]) == 0xbb);

    // Connected devices & protocol version
    auto devs = connector.getConnectedDevices();
    REQUIRE(devs.size() == 1);
    REQUIRE(devs[0].name == "Sony XM5");
    REQUIRE(devs[0].mac == "55:44:33:22:11:00");
    REQUIRE(connector.getProtocolVersion() == SonyProtocolVersion::V2);

    connector.disconnect();
    REQUIRE_FALSE(connector.isConnected());
}

TEST_CASE("Integration: BluetoothWrapper functions end-to-end over FakeTransport", "[transport][integration]")
{
    auto fakeTransport = std::make_unique<FakeTransport>();
    FakeTransport* rawFake = fakeTransport.get();

    auto connector = std::make_unique<TransportBluetoothConnector>(std::move(fakeTransport));
    BluetoothWrapper wrapper(std::move(connector));

    wrapper.connect("11:22:33:44:55:66");
    REQUIRE(wrapper.isConnected());

    // 1. sendCommand sends an escaped MDR frame into FakeTransport and awaits an ACK
    auto ack0 = CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, 0);
    rawFake->queueIncoming(std::vector<uint8_t>(ack0.begin(), ack0.end()));

    std::vector<char> testPayload = {0x02, 0x01};
    wrapper.sendCommand(testPayload);

    REQUIRE(rawFake->sentCount() == 1);
    const auto& sentFrame = rawFake->lastSentFrame();
    REQUIRE(static_cast<char>(sentFrame.front()) == START_MARKER);
    REQUIRE(static_cast<char>(sentFrame.back()) == END_MARKER);

    // 2. sendCommandAndReadResponse: verify round trip with fake transport
    // Device responds with an ACK for the command, then a DATA_MDR response frame
    std::vector<char> respPayload = {0x02, 0x02, 0x50};
    auto ack1 = CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, 1);
    auto responseFrame = CommandSerializer::packageDataForBt(respPayload, DATA_TYPE::DATA_MDR, 1);

    rawFake->queueIncoming(std::vector<uint8_t>(ack1.begin(), ack1.end()));
    rawFake->queueIncoming(std::vector<uint8_t>(responseFrame.begin(), responseFrame.end()));

    auto result = wrapper.sendCommandAndReadResponse({0x02, 0x01}, 0x02);
    REQUIRE(result == respPayload);

    // The wrapper must have automatically ACKed the received DATA_MDR frame back to the fake transport
    REQUIRE(rawFake->sentCount() >= 3);
    const auto& hostAckFrame = rawFake->lastSentFrame();
    auto unpackedHostAck = CommandSerializer::unpackBtMessage(Buffer(hostAckFrame.begin() + 1, hostAckFrame.end() - 1));
    REQUIRE(unpackedHostAck.dataType == DATA_TYPE::ACK);

    wrapper.disconnect();
    REQUIRE_FALSE(wrapper.isConnected());
}

TEST_CASE("Platform transport aliases instantiate properly", "[transport][platform]")
{
    auto mock = std::make_unique<MockConnector>();
    LinuxBluetoothTransport linuxTransport(std::move(mock));
    REQUIRE_FALSE(linuxTransport.isConnected());

    auto mock2 = std::make_unique<MockConnector>();
    WindowsBluetoothTransport winTransport(std::move(mock2));
    REQUIRE_FALSE(winTransport.isConnected());

    auto mock3 = std::make_unique<MockConnector>();
    MacOSBluetoothTransport macTransport(std::move(mock3));
    REQUIRE_FALSE(macTransport.isConnected());
}
