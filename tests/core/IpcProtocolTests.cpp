#include <catch2/catch_test_macros.hpp>
#include "sony/core/IpcProtocol.h"
#include "sony/core/IpcServer.h"
#include "sony/core/IpcClient.h"
#include "sony/core/DeviceService.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"

#include <algorithm>
#include "../support/PrivateSocket.h"

using namespace sony;
using namespace sony::core;
using namespace sony::protocol;
using namespace sony::transport;

namespace {

class AutoAckFakeTransport : public FakeTransport {
public:
    size_t send(std::span<const std::byte> data) override {
        size_t res = FakeTransport::send(data);
        if (!data.empty()) {
            std::vector<uint8_t> bytes(data.size());
            std::transform(data.begin(), data.end(), bytes.begin(), [](std::byte b) {
                return static_cast<uint8_t>(b);
            });
            try {
                auto frame = FrameCodec::decode(bytes);
                if (frame.type == DataType::DataMdr) {
                    SonyFrame ackFrame{
                        .type = DataType::Ack,
                        .sequence = frame.sequence,
                        .payload = {}
                    };
                    queueIncoming(FrameCodec::encode(ackFrame));
                    // Auto-respond to known inquiry requests
                    if (!frame.payload.empty()) {
                        uint8_t op = frame.payload[0];
                        if (op == 0x22) { // Battery query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = 1,
                                .payload = {0x23, 0x00, 85, 0x00}
                            }));
                        } else if (op == 0x66) { // NC query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = 1,
                                .payload = {0x67, 0x17, 0x01, 0x01, 0x00, 0x00, 0x00}
                            }));
                        } else if (op == 0x56) { // EQ query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = 1,
                                .payload = {0x57, 0x00, 0x00, 0x06, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a}
                            }));
                        } else if (op == 0xe6) { // DSEE query
                            queueIncoming(FrameCodec::encode(SonyFrame{
                                .type = DataType::DataMdr,
                                .sequence = 1,
                                .payload = {0xe7, 0x01, 0x00}
                            }));
                        }
                    }
                }
            } catch (...) {}
        }
        return res;
    }
};

} // namespace

TEST_CASE("IpcProtocol parses CLI command strings", "[core][ipc]") {
    SECTION("devices command") {
        auto cmd = IpcProtocol::parseCommand("devices");
        CHECK(cmd.type == IpcCommandType::Devices);
        CHECK(cmd.args.empty());
    }

    SECTION("info command") {
        auto cmd = IpcProtocol::parseCommand("info");
        CHECK(cmd.type == IpcCommandType::Info);
    }

    SECTION("battery command") {
        auto cmd = IpcProtocol::parseCommand("battery");
        CHECK(cmd.type == IpcCommandType::Battery);
    }

    SECTION("anc commands") {
        auto cmdOn = IpcProtocol::parseCommand("anc on");
        CHECK(cmdOn.type == IpcCommandType::Anc);
        REQUIRE(cmdOn.args.size() == 1);
        CHECK(cmdOn.args[0] == "on");

        auto cmdOff = IpcProtocol::parseCommand("anc off");
        CHECK(cmdOff.type == IpcCommandType::Anc);
        REQUIRE(cmdOff.args.size() == 1);
        CHECK(cmdOff.args[0] == "off");
    }

    SECTION("ambient commands") {
        auto cmd = IpcProtocol::parseCommand("ambient 12");
        CHECK(cmd.type == IpcCommandType::Ambient);
        REQUIRE(cmd.args.size() == 1);
        CHECK(cmd.args[0] == "12");

        auto cmdOff = IpcProtocol::parseCommand("ambient off");
        CHECK(cmdOff.type == IpcCommandType::Ambient);
        CHECK(cmdOff.args[0] == "off");
    }

    SECTION("equalizer commands") {
        auto cmdGet = IpcProtocol::parseCommand("eq get");
        CHECK(cmdGet.type == IpcCommandType::EqGet);

        auto cmdPreset = IpcProtocol::parseCommand("eq preset bass-boost");
        CHECK(cmdPreset.type == IpcCommandType::EqPreset);
        REQUIRE(cmdPreset.args.size() == 2);
        CHECK(cmdPreset.args[1] == "bass-boost");

        auto cmdPresetShorthand = IpcProtocol::parseCommand("eq bright");
        CHECK(cmdPresetShorthand.type == IpcCommandType::EqPreset);
        REQUIRE(cmdPresetShorthand.args.size() == 1);
        CHECK(cmdPresetShorthand.args[0] == "bright");

        auto cmdCustom = IpcProtocol::parseCommand("eq custom 3 1 2 3 4 5");
        CHECK(cmdCustom.type == IpcCommandType::EqCustom);
        REQUIRE(cmdCustom.args.size() == 7);
    }

    SECTION("dsee and status commands") {
        auto cmdDsee = IpcProtocol::parseCommand("dsee on");
        CHECK(cmdDsee.type == IpcCommandType::Dsee);
        CHECK(cmdDsee.args[0] == "on");

        auto cmdStatus = IpcProtocol::parseCommand("status");
        CHECK(cmdStatus.type == IpcCommandType::Status);

        auto cmdApo = IpcProtocol::parseCommand("autopoweroff 3");
        CHECK(cmdApo.type == IpcCommandType::AutoPowerOff);
    }
}

TEST_CASE("IpcProtocol serialization and parsing roundtrip", "[core][ipc]") {
    IpcResponse resp{
        .success = true,
        .message = "Battery status",
        .data = "Battery: 85% (Charging)"
    };

    auto serialized = IpcProtocol::serializeResponse(resp);
    auto parsed = IpcProtocol::parseResponse(serialized);

    CHECK(parsed.success == resp.success);
    CHECK(parsed.message == resp.message);
    CHECK(parsed.data == resp.data);

    IpcResponse errResp{
        .success = false,
        .message = "No device connected",
        .data = ""
    };

    auto errSerialized = IpcProtocol::serializeResponse(errResp);
    auto errParsed = IpcProtocol::parseResponse(errSerialized);

    CHECK(errParsed.success == false);
    CHECK(errParsed.message == "No device connected");
}

TEST_CASE("IpcProtocol execution through DeviceService", "[core][ipc]") {
    auto transport = std::make_shared<AutoAckFakeTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    discovery->addDevice(transport::DiscoveredDevice{
        .name = "WH-1000XM5",
        .address = DeviceAddress("11:22:33:44:55:66")
    });

    DeviceService service(transport, discovery);

    SECTION("devices command returns discovered devices") {
        auto cmd = IpcProtocol::parseCommand("devices");
        auto resp = IpcProtocol::execute(cmd, service);
        CHECK(resp.success);
        CHECK(resp.message == "1 devices found");
        CHECK(resp.data.find("WH-1000XM5 [11:22:33:44:55:66]") != std::string::npos);
    }

    SECTION("commands fail cleanly when no device is connected") {
        auto cmd = IpcProtocol::parseCommand("anc on");
        auto resp = IpcProtocol::execute(cmd, service);
        CHECK_FALSE(resp.success);
        CHECK(resp.message == "No device connected");
    }

    SECTION("connected device executes commands") {
        service.connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM5");
        REQUIRE(service.isConnected());

        // Status
        auto statusResp = IpcProtocol::execute(IpcProtocol::parseCommand("status"), service);
        CHECK(statusResp.success);
        CHECK(statusResp.data.find("WH-1000XM5") != std::string::npos);

        // Info
        auto infoResp = IpcProtocol::execute(IpcProtocol::parseCommand("info"), service);
        CHECK(infoResp.success);
        CHECK(infoResp.data.find("WH-1000XM5") != std::string::npos);
        CHECK(infoResp.data.find("ANC") != std::string::npos);

        // ANC
        auto ancResp = IpcProtocol::execute(IpcProtocol::parseCommand("anc on"), service);
        CHECK(ancResp.success);
        CHECK(service.snapshot()->noiseControl.mode == NoiseControlMode::NoiseCancelling);

        auto ancOffResp = IpcProtocol::execute(IpcProtocol::parseCommand("anc off"), service);
        CHECK(ancOffResp.success);
        CHECK(service.snapshot()->noiseControl.mode == NoiseControlMode::Off);

        // Ambient
        auto ambResp = IpcProtocol::execute(IpcProtocol::parseCommand("ambient 14"), service);
        CHECK(ambResp.success);
        CHECK(service.snapshot()->noiseControl.mode == NoiseControlMode::Ambient);
        CHECK(service.snapshot()->noiseControl.ambientLevel == 14);

        // EQ Preset
        auto eqResp = IpcProtocol::execute(IpcProtocol::parseCommand("eq preset bass-boost"), service);
        CHECK(eqResp.success);
        CHECK(service.snapshot()->equalizer.preset == 0x16);

        for (const auto* preset : {"256", "272"}) {
            auto invalidEqResp = IpcProtocol::execute(
                IpcProtocol::parseCommand(std::string("eq preset ") + preset), service);
            CHECK_FALSE(invalidEqResp.success);
            CHECK(invalidEqResp.message == std::string("Unknown preset: ") + preset);
            CHECK(service.snapshot()->equalizer.preset == 0x16);
        }

        // EQ Get
        auto eqGetResp = IpcProtocol::execute(IpcProtocol::parseCommand("eq get"), service);
        CHECK(eqGetResp.success);
        CHECK(eqGetResp.data.find("Bass Boost") != std::string::npos);

        // EQ Custom
        auto eqCustResp = IpcProtocol::execute(IpcProtocol::parseCommand("eq custom 5 1 2 3 4 5"), service);
        CHECK(eqCustResp.success);
        CHECK(service.snapshot()->equalizer.clearBass == 5);
        CHECK(service.snapshot()->equalizer.bands[0] == 1);
        CHECK(service.snapshot()->equalizer.bands[4] == 5);

        // DSEE
        auto dseeResp = IpcProtocol::execute(IpcProtocol::parseCommand("dsee on"), service);
        CHECK(dseeResp.success);
        CHECK(service.snapshot()->dsee == true);

        // Auto Power Off
        auto apoResp = IpcProtocol::execute(IpcProtocol::parseCommand("apo 3"), service);
        CHECK(apoResp.success);
        CHECK(service.snapshot()->autoPowerOff == 3);

        service.disconnect();
        CHECK_FALSE(service.isConnected());
    }

    SECTION("WH-1000XM6 uses the 10-band equalizer, not Clear Bass") {
        service.connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM6");
        REQUIRE(service.isConnected());

        auto infoResp = IpcProtocol::execute(IpcProtocol::parseCommand("info"), service);
        CHECK(infoResp.success);
        CHECK(infoResp.data.find("Equalizer (10-band)") != std::string::npos);
        CHECK(infoResp.data.find("Clear Bass") == std::string::npos);

        // Too few values is rejected rather than silently defaulting the
        // rest to an unverified "flat" value.
        auto shortResp = IpcProtocol::execute(IpcProtocol::parseCommand("eq custom 1 2 3"), service);
        CHECK_FALSE(shortResp.success);

        auto custResp = IpcProtocol::execute(
            IpcProtocol::parseCommand("eq custom 7 7 7 7 6 6 3 7 3 8"), service);
        CHECK(custResp.success);
        CHECK(service.snapshot()->equalizer.bands == std::vector<int>{7, 7, 7, 7, 6, 6, 3, 7, 3, 8});

        auto eqGetResp = IpcProtocol::execute(IpcProtocol::parseCommand("eq get"), service);
        CHECK(eqGetResp.success);
        CHECK(eqGetResp.data.find("Clear Bass") == std::string::npos);

        service.disconnect();
    }
}

TEST_CASE("IpcServer and IpcClient end-to-end communication over socket", "[core][ipc]") {
#ifdef _WIN32
    SKIP("Unix IPC is not implemented on Windows");
#endif
    auto transport = std::make_shared<AutoAckFakeTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    auto service = std::make_shared<DeviceService>(transport, discovery);
    service->connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM5");

    PrivateSocket socket;
    std::string testSocket = socket.path;

    IpcClient client(testSocket);
    CHECK_FALSE(client.isDaemonRunning());

    IpcServer server(service, testSocket);
    server.start();
    CHECK(server.isRunning());

    // Allow server thread to enter accept loop
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(client.isDaemonRunning());

    auto resp = client.sendCommand("battery");
    CHECK(resp.success);
    CHECK(resp.data.find("85%") != std::string::npos);

    auto ancResp = client.sendCommand("anc on");
    CHECK(ancResp.success);
    CHECK(service->snapshot()->noiseControl.mode == NoiseControlMode::NoiseCancelling);

    auto ambResp = client.sendCommand("ambient 12");
    CHECK(ambResp.success);
    CHECK(service->snapshot()->noiseControl.mode == NoiseControlMode::Ambient);
    CHECK(service->snapshot()->noiseControl.ambientLevel == 12);

    server.stop();
    CHECK_FALSE(server.isRunning());
    CHECK_FALSE(client.isDaemonRunning());
}
