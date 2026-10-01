#include <catch2/catch_test_macros.hpp>
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
    REQUIRE_FALSE(device.capabilities().battery);
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
