#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "sony/core/SonyDevice.h"
#include "sony/transport/FakeTransport.h"
#include "sony/protocol/FrameCodec.h"
#include <atomic>
using namespace sony;
using namespace sony::protocol;
using namespace sony::transport;
namespace {
class BleReplies : public FakeTransport {
    unsigned sequence{0};
public:
    bool ignoreSet{false};
    uint8_t failOpcode{0xff};
    ControlBearer bearer{ControlBearer::BleGatt};
    DeviceMetadata metadata{42, "LC3", "3.1.5"};
    bool failMetadata{false};
    unsigned metadataReads{0};
    DeviceMetadata deviceMetadata() override {
        ++metadataReads;
        if (failMetadata) throw SonyException(SonyErrorCode::TransportFailure, "BlueZ unavailable");
        return metadata;
    }
    std::vector<uint8_t> noise{0x67,0x19,1,1,1,0,12,0,0};
    ControlBearer controlBearer() const noexcept override { return bearer; }
    size_t send(std::span<const std::byte> bytes) override {
        auto count=FakeTransport::send(bytes);
        std::vector<uint8_t> packet;
        for (auto b:bytes) packet.push_back(static_cast<uint8_t>(b));
        auto frame=FrameCodec::decode(packet);
        if (frame.type!=DataType::DataMdr) return count;
        if (frame.payload[0] == failOpcode)
            throw SonyException(SonyErrorCode::Timeout, "Injected control timeout");
        queueIncoming(FrameCodec::encode({DataType::Ack,static_cast<uint8_t>(frame.sequence^1),{}}));
        std::vector<uint8_t> reply;
        if (frame.payload[0]==0) reply={1,0};
        if (frame.payload[0]==0x66) reply=noise;
        if (frame.payload[0]==0x68 && !ignoreSet) { noise=frame.payload; noise[0]=0x67; }
        if (bearer != ControlBearer::BleGatt) {
            if (frame.payload[0] == 0x22) reply = {0x23,0,85,0};
            if (frame.payload[0] == 0x12) reply = {0x13,2,0x10};
            if (frame.payload[0] == 0x04) reply = {0x05,2,5,'9','.','9','.','9'};
            if (frame.payload[0] == 0xe6) reply = {0xe7,1,0};
            if (frame.payload[0] == 0x56) {
                reply = {0x57,frame.payload[1],0,static_cast<uint8_t>(frame.payload[1] == 4 ? 10 : 6)};
                reply.resize(4 + reply[3], 10);
            }
        }
        if (!reply.empty()) queueIncoming(FrameCodec::encode({DataType::DataMdr,static_cast<uint8_t>(sequence++%2),reply}));
        return count;
    }
};
}
TEST_CASE("XM6 BLE session limits polling and confirms noise changes", "[core][ble]") {
    auto transport=std::make_shared<BleReplies>();
    core::SonyDevice device(transport);
    device.connect("11:22:33:44:55:66","WH-1000XM6");
    REQUIRE(device.isConnected());
    REQUIRE(device.capabilities().noiseCancelling);
    REQUIRE(device.capabilities().battery);
    REQUIRE_FALSE(device.capabilities().equalizer);
    REQUIRE(device.state().noiseControl.mode==NoiseControlMode::Ambient);
    SECTION("Confirmed Off") {
        device.setAnc(false);
        REQUIRE(device.state().noiseControl.mode==NoiseControlMode::Off);
        REQUIRE(device.state().features.at("noiseControl").availability=="valid");
    }
    SECTION("Unconfirmed command retains previous state") {
        transport->ignoreSet=true;
        REQUIRE_THROWS(device.setAnc(false));
        REQUIRE(device.state().noiseControl.mode==NoiseControlMode::Ambient);
        REQUIRE(device.state().features.at("noiseControl").availability=="stale");
    }
    SECTION("Failed inquiry marks noise control stale") {
        transport->failOpcode = 0x66;
        REQUIRE_THROWS(device.setAnc(false));
        REQUIRE(device.state().features.at("noiseControl").availability == "stale");
        REQUIRE(device.state().noiseControl.mode == NoiseControlMode::Ambient);
        transport->failOpcode = 0xff;
    }
    SECTION("Failed SET marks noise control stale") {
        transport->failOpcode = 0x68;
        REQUIRE_THROWS(device.setAnc(false));
        REQUIRE(device.state().features.at("noiseControl").availability == "stale");
        REQUIRE(device.state().noiseControl.mode == NoiseControlMode::Ambient);
        transport->failOpcode = 0xff;
    }
    SECTION("Unsupported controls do not send packets") {
        auto count=transport->sentCount();
        REQUIRE_THROWS(device.setEqualizerPreset(0));
        REQUIRE_THROWS(device.setDsee(true));
        REQUIRE_THROWS(device.setSpeakToChat(true));
        REQUIRE_THROWS(device.setAutoPowerOff(1));
        REQUIRE_THROWS(device.setAdaptiveVolume(true));
        REQUIRE(transport->sentCount()==count);
    }
    for (int step=0;step<9;++step) device.refreshSettingsStep();
    for (const auto& packet:transport->sentFrames()) {
        auto frame=FrameCodec::decode(packet);
        if (frame.type==DataType::DataMdr) REQUIRE((frame.payload[0]==0 || frame.payload[0]==0x66 || frame.payload[0]==0x68));
    }
}

TEST_CASE("XM6 BLE metadata stays independent of Sony protocol polling", "[core][ble]") {
    auto transport = std::make_shared<BleReplies>();
    core::SonyDevice device(transport);
    device.connect("11:22:33:44:55:66", "WH-1000XM6");
    REQUIRE(device.state().battery.main == 42);
    REQUIRE_FALSE(device.state().battery.charging);
    REQUIRE(device.state().codec == "LC3");
    REQUIRE(device.state().firmware == "3.1.5");
    for (const auto* feature : {"battery", "codec", "firmware"})
        REQUIRE(device.state().features.at(feature).availability == "valid");
    SECTION("Zero battery is valid and metadata is refreshed") {
        transport->metadata.batteryPercentage = 0;
        device.refreshBattery();
        REQUIRE(device.state().battery.main == 0);
        REQUIRE(device.state().features.at("battery").availability == "valid");
    }
    SECTION("Stopped audio and missing battery invalidate previous readings") {
        transport->metadata = {{}, {}, "3.1.5"};
        for (int i = 0; i < 4; ++i) device.refreshSettingsStep();
        REQUIRE_FALSE(device.state().battery.main);
        REQUIRE(device.state().codec.empty());
        REQUIRE(device.state().features.at("battery").availability == "stale");
        REQUIRE(device.state().features.at("codec").availability == "stale");
        REQUIRE(device.state().features.at("firmware").availability == "valid");
    }
    SECTION("BlueZ failure does not close the control connection") {
        transport->failMetadata = true;
        device.refreshBattery();
        REQUIRE(device.isConnected());
        for (const auto* feature : {"battery", "codec", "firmware"})
            REQUIRE(device.state().features.at(feature).availability == "stale");
        transport->failMetadata = false;
        device.refreshBattery();
        REQUIRE(device.state().features.at("battery").availability == "valid");
    }
    SECTION("Missing initial metadata remains unknown") {
        device.disconnect();
        transport->metadata = {};
        device.connect("11:22:33:44:55:66", "WH-1000XM6");
        for (const auto* feature : {"battery", "codec", "firmware"})
            REQUIRE(device.state().features.at(feature).availability == "unknown");
    }
    SECTION("Disconnect invalidates all metadata") {
        device.disconnect();
        for (const auto* feature : {"battery", "codec", "firmware"})
            REQUIRE(device.state().features.at(feature).availability == "stale");
    }
    for (const auto& packet : transport->sentFrames()) {
        auto frame = FrameCodec::decode(packet);
        if (frame.type == DataType::DataMdr) REQUIRE((frame.payload[0] == 0 || frame.payload[0] == 0x66));
    }
}

TEST_CASE("Classic metadata keeps using MDR after BLE control", "[core][ble][regression]") {
    const auto bearer = GENERATE(ControlBearer::Rfcomm, ControlBearer::Unknown);
    const auto model = GENERATE("WH-1000XM5", "WH-1000XM6");
    auto transport = std::make_shared<BleReplies>();
    core::SonyDevice device(transport);
    SECTION("Classic from startup") {}
    SECTION("Reconnect from BLE") {
        device.connect("11:22:33:44:55:66", "WH-1000XM6");
        REQUIRE(device.state().codec == "LC3");
        device.disconnect();
    }
    transport->bearer = bearer;
    transport->noise = {0x67,0x17,1,1,0,0,0};
    transport->metadataReads = 0;
    transport->failMetadata = true;
    device.connect("11:22:33:44:55:66", model);
    for (int step = 0; step < 5; ++step) device.refreshSettingsStep();
    REQUIRE(transport->metadataReads == 0);
    REQUIRE(device.state().battery.main == 85);
    REQUIRE(device.state().codec == "LDAC");
    REQUIRE(device.state().firmware == "9.9.9");
    REQUIRE(device.capabilities().equalizer);
    for (const auto* feature : {"battery", "codec", "firmware", "equalizer"})
        REQUIRE(device.state().features.at(feature).availability == "valid");
}
