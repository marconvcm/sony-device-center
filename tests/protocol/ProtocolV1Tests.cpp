#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/ProtocolV1.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"

using namespace sony;
using namespace sony::protocol;
using namespace sony::transport;

namespace {

// First DataMdr payload the host transmitted (ACK frames are skipped).
std::vector<uint8_t> firstRequestPayload(const FakeTransport& fake) {
    for (const auto& frameBytes : fake.sentFrames()) {
        auto decoded = FrameCodec::decode(frameBytes);
        if (decoded.type == DataType::DataMdr) return decoded.payload;
    }
    return {};
}

void queueReply(FakeTransport& fake, std::vector<uint8_t> payload) {
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 1, .payload = std::move(payload) }));
}

} // namespace

TEST_CASE("ProtocolV1: explicit regression - battery request must NEVER send opcode 0x22", "[protocol][v1][regression]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    ProtocolV1 v1(session);
    REQUIRE(v1.generation() == ProtocolGeneration::V1);

    // Single reading: 11 00 <50%> <charging>
    queueReply(fake, {0x11, 0x00, 50, 1});
    auto battery = v1.getBattery();

    // Verify opcode 0x22 was NOT sent in ANY transmitted frame
    for (const auto& frameBytes : fake.sentFrames()) {
        auto decoded = FrameCodec::decode(frameBytes);
        if (decoded.type == DataType::DataMdr && !decoded.payload.empty()) {
            REQUIRE(decoded.payload[0] != 0x22);
        }
    }

    // The legacy battery request is 10 00 and the reading is decoded from it
    REQUIRE(firstRequestPayload(fake) == std::vector<uint8_t>{0x10, 0x00});
    REQUIRE(battery.main.has_value());
    REQUIRE(*battery.main == 50);
    REQUIRE(battery.charging);
    REQUIRE_FALSE(battery.left.has_value());
    REQUIRE_FALSE(battery.caseBattery.has_value());
}

TEST_CASE("ProtocolV1: an over-ear dual reply with an empty right slot is a single reading", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV1 v1(session);

    // Single query is acknowledged but never answered; the dual query then
    // comes back as 11 01 <L=50> <chg> <R=0> <chg>, which is how a WH-1000XM4
    // echoes its one cell.
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 1 }));
    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::DataMdr, .sequence = 0, .payload = {0x11, 0x01, 50, 0, 0, 0} }));

    auto battery = v1.getBattery();
    REQUIRE(battery.main.has_value());
    REQUIRE(*battery.main == 50);
    REQUIRE_FALSE(battery.left.has_value());
    REQUIRE_FALSE(battery.right.has_value());
}

TEST_CASE("ProtocolV1: decodes noise-control readback", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV1 v1(session);

    SECTION("ambient sound with focus on voice")
    {
        // 67 02 <effect on> <ncType> <dualSingle off = ambient> <asmType> <voice> <level 5>
        queueReply(fake, {0x67, 0x02, 0x01, 0x02, 0x00, 0x01, 0x01, 5});
        auto nc = v1.getNoiseControl();
        REQUIRE(firstRequestPayload(fake) == std::vector<uint8_t>{0x66, 0x02});
        REQUIRE(nc.mode == NoiseControlMode::Ambient);
        REQUIRE(nc.ambientLevel == 5);
        REQUIRE(nc.focusOnVoice);
    }

    SECTION("noise cancelling")
    {
        queueReply(fake, {0x67, 0x02, 0x01, 0x02, 0x02, 0x01, 0x00, 0});
        auto nc = v1.getNoiseControl();
        REQUIRE(nc.mode == NoiseControlMode::NoiseCancelling);
        REQUIRE(nc.ambientLevel == 0);
        REQUIRE_FALSE(nc.focusOnVoice);
    }

    SECTION("off keeps no ambient level")
    {
        queueReply(fake, {0x67, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00, 3});
        auto nc = v1.getNoiseControl();
        REQUIRE(nc.mode == NoiseControlMode::Off);
        REQUIRE(nc.ambientLevel == 0);
    }

    SECTION("a truncated reply is rejected")
    {
        queueReply(fake, {0x67, 0x02, 0x01});
        REQUIRE_THROWS_AS(v1.getNoiseControl(), SonyException);
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

    SECTION("ambient sound carries the level and focus-on-voice")
    {
        v1.setNoiseControl({ .mode = NoiseControlMode::Ambient, .ambientLevel = 7, .focusOnVoice = true });
        REQUIRE(fake.sentCount() == 1);
        auto sent = FrameCodec::decode(fake.lastSentFrame());
        REQUIRE(sent.type == DataType::DataMdr);
        REQUIRE(sent.payload == std::vector<uint8_t>{0x68, 0x02, 0x11, 0x01, 0x00, 0x01, 0x01, 7});
    }

    SECTION("noise cancelling is dual NC with the level at zero")
    {
        v1.setNoiseControl({ .mode = NoiseControlMode::NoiseCancelling, .ambientLevel = 7, .focusOnVoice = false });
        auto sent = FrameCodec::decode(fake.lastSentFrame());
        REQUIRE(sent.payload == std::vector<uint8_t>{0x68, 0x02, 0x11, 0x01, 0x02, 0x01, 0x00, 0});
    }

    SECTION("off clears the effect byte")
    {
        v1.setNoiseControl({ .mode = NoiseControlMode::Off, .ambientLevel = 7, .focusOnVoice = false });
        auto sent = FrameCodec::decode(fake.lastSentFrame());
        REQUIRE(sent.payload == std::vector<uint8_t>{0x68, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00, 0});
    }

    SECTION("ambient level is clamped to the 1-20 range")
    {
        v1.setNoiseControl({ .mode = NoiseControlMode::Ambient, .ambientLevel = 0, .focusOnVoice = false });
        auto sent = FrameCodec::decode(fake.lastSentFrame());
        REQUIRE(sent.payload[7] == 1);
    }
}

TEST_CASE("ProtocolV1: reads the equalizer behind inquired type 0x01", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV1 v1(session);

    // 57 01 <Bass Boost> 06 <bass +5> <bands 0 1 2 3 4>
    queueReply(fake, {0x57, 0x01, 0x16, 0x06, 15, 10, 11, 12, 13, 14});
    auto eq = v1.getEqualizer();

    REQUIRE(firstRequestPayload(fake) == std::vector<uint8_t>{0x56, 0x01});
    REQUIRE(eq.preset == 0x16);
    REQUIRE(eq.clearBass == 5);
    REQUIRE(eq.bands == std::array<int, 5>{0, 1, 2, 3, 4});
}

TEST_CASE("ProtocolV1: writes equalizer presets and custom bands", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV1 v1(session);

    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 0 }));
    v1.setEqualizerPreset(0x16);
    REQUIRE(FrameCodec::decode(fake.sentFrames()[0]).payload == std::vector<uint8_t>{0x58, 0x01, 0x16, 0x00});

    fake.queueIncoming(FrameCodec::encode(SonyFrame{ .type = DataType::Ack, .sequence = 1 }));
    v1.setEqualizerCustom(5, {-10, 10, 0, 3, -3});
    REQUIRE(FrameCodec::decode(fake.sentFrames()[1]).payload
            == std::vector<uint8_t>{0x58, 0x01, 0xa0, 0x06, 15, 0, 20, 10, 13, 7});
}

TEST_CASE("ProtocolV1: reads firmware version and codec", "[protocol][v1]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV1 v1(session);

    SECTION("firmware is the ASCII tail after the length byte")
    {
        queueReply(fake, {0x05, 0x02, 0x05, '3', '.', '0', '.', '1'});
        REQUIRE(v1.getFirmwareVersion() == "3.0.1");
        REQUIRE(firstRequestPayload(fake) == std::vector<uint8_t>{0x04, 0x02});
    }

    SECTION("codec byte maps to its name")
    {
        queueReply(fake, {0x19, 0x00, 0x02});
        REQUIRE(v1.getCodec() == "AAC");
        REQUIRE(firstRequestPayload(fake) == std::vector<uint8_t>{0x18, 0x00});
    }
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

    REQUIRE_THROWS_AS(v1.getDsee(), SonyException);
    REQUIRE_THROWS_AS(v1.setDsee(true), SonyException);
    REQUIRE_THROWS_AS(v1.getSpeakToChat(), SonyException);
    REQUIRE_THROWS_AS(v1.getAdaptiveVolume(), SonyException);
    REQUIRE_THROWS_AS(v1.getAutoPowerOff(), SonyException);
    REQUIRE(fake.sentCount() == 0);
}
