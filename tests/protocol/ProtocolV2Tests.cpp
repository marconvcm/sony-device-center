#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/ProtocolV2.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"

using namespace sony;
using namespace sony::protocol;
using namespace sony::transport;

TEST_CASE("ProtocolV2: uses opcode 0x22 for battery request", "[protocol][v2]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV2 v2(session);
    REQUIRE(v2.generation() == ProtocolGeneration::V2);

    // Reply for single battery: 23 00 80 01 (80%, charging)
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 1, .payload = {0x23, 0x00, 80, 1} }));

    auto battery = v2.getBattery();
    REQUIRE(battery.main.has_value());
    REQUIRE(*battery.main == 80);
    REQUIRE(battery.charging == true);

    // Verify opcode 0x22 WAS sent
    bool found0x22 = false;
    for (const auto& frameBytes : fake.sentFrames()) {
        auto decoded = FrameCodec::decode(frameBytes);
        if (decoded.type == DataType::DataMdr && !decoded.payload.empty() && decoded.payload[0] == 0x22) {
            found0x22 = true;
            break;
        }
    }
    REQUIRE(found0x22);
}

TEST_CASE("ProtocolV2: handles noise control query and command", "[protocol][v2]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV2 v2(session);

    SECTION("getNoiseControl decodes ambient mode")
    {
        // Reply: 67 17 01 <on=1> <ambient=1> <voice=0> <level=12>
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 1, .payload = {0x67, 0x17, 0x01, 1, 1, 0, 12} }));

        auto nc = v2.getNoiseControl();
        REQUIRE(nc.mode == NoiseControlMode::Ambient);
        REQUIRE(nc.ambientLevel == 12);
        REQUIRE(nc.focusOnVoice == false);
    }

    SECTION("setNoiseControl sends V2 layout")
    {
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));

        NoiseControlState state{
            .mode = NoiseControlMode::NoiseCancelling,
            .ambientLevel = 0,
            .focusOnVoice = false
        };
        v2.setNoiseControl(state);

        REQUIRE(fake.sentCount() == 1);
        auto sent = FrameCodec::decode(fake.lastSentFrame());
        REQUIRE(sent.payload == std::vector<uint8_t>{0x68, 0x17, 0x01, 1, 0, 0, 1});
    }
}

TEST_CASE("ProtocolV2: handles equalizer queries and settings", "[protocol][v2]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV2 v2(session);

    SECTION("getEqualizer decodes preset and bands")
    {
        // Reply: 57 00 <preset=0x16: Bass Boost> 06 <clearBass=13 (+3)> <10, 11, 12, 13, 14 (0, +1, +2, +3, +4)>
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 1, .payload = {0x57, 0x00, 0x16, 0x06, 13, 10, 11, 12, 13, 14} }));

        auto eq = v2.getEqualizer();
        REQUIRE(eq.preset == 0x16);
        REQUIRE(eq.clearBass == 3);
        REQUIRE(eq.bands == std::array<int, 5>{0, 1, 2, 3, 4});
    }

    SECTION("setEqualizerPreset sends command")
    {
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
        v2.setEqualizerPreset(0x16);

        REQUIRE(fake.sentCount() == 1);
        auto sent = FrameCodec::decode(fake.lastSentFrame());
        REQUIRE(sent.payload == std::vector<uint8_t>{0x58, 0x00, 0x16, 0x00});
    }

    SECTION("setEqualizerCustom sends clamped bands")
    {
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
        v2.setEqualizerCustom(5, { -2, 0, 3, 7, 10 });

        REQUIRE(fake.sentCount() == 1);
        auto sent = FrameCodec::decode(fake.lastSentFrame());
        REQUIRE(sent.payload == std::vector<uint8_t>{0x58, 0x00, 0xa0, 0x06, 15, 8, 10, 13, 17, 20});
    }
}

TEST_CASE("ProtocolV2: handles DSEE query and control", "[protocol][v2]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV2 v2(session);

    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 1, .payload = {0xe7, 0x01, 0x01} }));

    bool dsee = v2.getDsee();
    REQUIRE(dsee == true);

    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 1 }));
    v2.setDsee(false);

    auto sent = FrameCodec::decode(fake.lastSentFrame());
    REQUIRE(sent.payload == std::vector<uint8_t>{0xe8, 0x01, 0x00});
}

TEST_CASE("ProtocolV2: handles peripheral feature inquiries", "[protocol][v2]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV2 v2(session);

    SECTION("firmware version")
    {
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 1, .payload = {0x05, 0x02, 0x00, '2', '.', '0', '.', '1'} }));

        auto fw = v2.getFirmwareVersion();
        REQUIRE(fw == "2.0.1");
    }

    SECTION("codec inquiry")
    {
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
        fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 1, .payload = {0x13, 0x02, 0x10} })); // 0x10 = LDAC

        auto codec = v2.getCodec();
        REQUIRE(codec == "LDAC");
    }
}
