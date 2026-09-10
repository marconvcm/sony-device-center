#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/ProtocolV1.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"

using namespace sony;
using namespace sony::protocol;
using namespace sony::transport;

TEST_CASE("ProtocolV1: explicit regression - battery request must NEVER send opcode 0x22", "[protocol][v1][regression]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV1 v1(session);
    REQUIRE(v1.generation() == ProtocolGeneration::V1);

    // Call getBattery()
    auto battery = v1.getBattery();

    // Verify opcode 0x22 was NOT sent in ANY transmitted frame
    for (const auto& frameBytes : fake.sentFrames()) {
        auto decoded = FrameCodec::decode(frameBytes);
        if (decoded.type == DataType::DataMdr && !decoded.payload.empty()) {
            REQUIRE(decoded.payload[0] != 0x22);
        }
    }
}

TEST_CASE("ProtocolV1: sets noise control with V1 packet layout", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    // Host sends command and awaits ACK
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));

    ProtocolV1 v1(session);
    NoiseControlState state{
        .mode = NoiseControlMode::Ambient,
        .ambientLevel = 7,
        .focusOnVoice = true
    };
    v1.setNoiseControl(state);

    REQUIRE(fake.sentCount() == 1);
    auto sentFrame = FrameCodec::decode(fake.lastSentFrame());
    REQUIRE(sentFrame.type == DataType::DataMdr);
    REQUIRE(sentFrame.payload.size() == 8);
    REQUIRE(sentFrame.payload[0] == 0x68); // NCASM_SET_PARAM
    REQUIRE(sentFrame.payload[1] == 0x02); // V1 NC_ASM_INQUIRED_TYPE
    REQUIRE(sentFrame.payload[2] == 17);   // ADJUSTMENT_COMPLETION
    REQUIRE(sentFrame.payload[6] == 1);    // focus on voice = 1
    REQUIRE(sentFrame.payload[7] == 7);    // level = 7
}

TEST_CASE("ProtocolV1: sends VPT and sound position commands", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV1 v1(session);

    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
    v1.setVpt(3); // Concert Hall

    REQUIRE(fake.sentCount() == 1);
    auto vptFrame = FrameCodec::decode(fake.sentFrames()[0]);
    REQUIRE(vptFrame.payload == std::vector<uint8_t>{0x48, 0x01, 0x03});

    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 1 }));
    v1.setSoundPosition(1); // Front Left

    REQUIRE(fake.sentCount() == 2);
    auto posFrame = FrameCodec::decode(fake.sentFrames()[1]);
    REQUIRE(posFrame.payload == std::vector<uint8_t>{0x48, 0x02, 0x01});
}

TEST_CASE("ProtocolV1: unsupported features throw Unsupported", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV1 v1(session);

    REQUIRE_THROWS_AS(v1.getEqualizer(), SonyException);
    REQUIRE_THROWS_AS(v1.setEqualizerPreset(1), SonyException);
    REQUIRE_THROWS_AS(v1.getDsee(), SonyException);
    REQUIRE_THROWS_AS(v1.setDsee(true), SonyException);
    REQUIRE_THROWS_AS(v1.getSpeakToChat(), SonyException);
    REQUIRE_THROWS_AS(v1.getAdaptiveVolume(), SonyException);
    REQUIRE_THROWS_AS(v1.getFirmwareVersion(), SonyException);
    REQUIRE_THROWS_AS(v1.getCodec(), SonyException);
    REQUIRE_THROWS_AS(v1.getAutoPowerOff(), SonyException);
}
