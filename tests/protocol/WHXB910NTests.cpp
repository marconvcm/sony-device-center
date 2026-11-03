// Regression fixtures from hardware-tested WH-XB910N commands:
// https://github.com/DaanHessen/sonyctl/blob/master/docs/protocol.md
#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/DeviceProfileRegistry.h"
#include "sony/protocol/DeviceEventDispatcher.h"
#include "sony/protocol/ProtocolV2.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"
#include "ReplyingFakeTransport.h"
using namespace sony;
using namespace sony::protocol;
using namespace sony::transport;
using sony::test::ReplyingFakeTransport;

TEST_CASE("WH-XB910N model and conservative capabilities", "[protocol][whxb910n]") {
    const auto p = DeviceProfileRegistry::getProfileForDevice("LE_WH-XB910N");
    CHECK(p.model == SonyModel::WHXB910N);
    CHECK(p.protocol == SonyProtocolVersion::V2);
    CHECK(p.capabilities.battery);
    CHECK(p.capabilities.noiseCancelling);
    CHECK(p.capabilities.ambientSound);
    CHECK(p.capabilities.focusOnVoice);
    CHECK(p.capabilities.equalizer);
    CHECK(p.capabilities.clearBass);
    CHECK_FALSE(p.capabilities.tenBandEqualizer);
    CHECK(p.capabilities.noiseControlLayout == NoiseControlLayout::WhXb910N);
    CHECK(p.capabilities.dsee);
    CHECK(p.capabilities.dseeInquiredType == 0);
    CHECK_FALSE(p.capabilities.speakToChat);
    CHECK_FALSE(p.capabilities.codecInfo);
    CHECK_FALSE(p.capabilities.autoPowerOff);
    CHECK(to_string(p.model) == "WH-XB910N");
    CHECK(DeviceProfileRegistry::getProfileForDevice("WH-1000XM5").capabilities.dseeInquiredType == 1);
}

TEST_CASE("WH-XB910N NC readback uses eight-byte subtype 0x16", "[protocol][whxb910n]") {
    ReplyingFakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV2 p(session, false, NoiseControlLayout::WhXb910N);
    fake.queueReply({
        SonyFrame{ .type = DataType::Ack, .sequence = 0 },
        SonyFrame{ .type = DataType::DataMdr, .sequence = 1,
                   .payload = {0x67, 0x16, 0x01, 0x01, 0x01, 0x02, 0x01, 0x14} }
    });
    const auto nc = p.getNoiseControl();
    CHECK(nc.mode == NoiseControlMode::Ambient);
    CHECK(nc.ambientLevel == 20);
    CHECK(nc.focusOnVoice);
    REQUIRE(fake.sentCount() >= 1);
    auto sent = FrameCodec::decode(fake.sentFrames().front());
    CHECK(sent.type == DataType::DataMdr);
    CHECK(sent.payload == std::vector<uint8_t>{0x66, 0x16});
}

TEST_CASE("WH-XB910N NC off, ANC, ambient setters use exact subtype 0x16", "[protocol][whxb910n]") {
    ReplyingFakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV2 p(session, false, NoiseControlLayout::WhXb910N);

    SECTION("off") {
        fake.queueReply({SonyFrame{.type=DataType::Ack, .sequence=0}});
        p.setNoiseControl({NoiseControlMode::Off, 0, false});
        CHECK(FrameCodec::decode(fake.lastSentFrame()).payload ==
              std::vector<uint8_t>{0x68,0x16,0x01,0x00,0x00,0x02,0x00,0x01});
    }
    SECTION("ANC") {
        fake.queueReply({SonyFrame{.type=DataType::Ack, .sequence=0}});
        p.setNoiseControl({NoiseControlMode::NoiseCancelling, 0, true});
        CHECK(FrameCodec::decode(fake.lastSentFrame()).payload ==
              std::vector<uint8_t>{0x68,0x16,0x01,0x01,0x00,0x02,0x00,0x01});
    }
    SECTION("ambient and clamping") {
        fake.queueReply({SonyFrame{.type=DataType::Ack, .sequence=0}});
        p.setNoiseControl({NoiseControlMode::Ambient, 255, true});
        CHECK(FrameCodec::decode(fake.lastSentFrame()).payload ==
              std::vector<uint8_t>{0x68,0x16,0x01,0x01,0x01,0x02,0x01,0x14});
    }
}

TEST_CASE("WH-XB910N hardware NC notifications decode correctly", "[events][whxb910n]") {
    DeviceEventDispatcher dispatcher;
    DeviceState state;
    CHECK(dispatcher.parseNotificationPayload({0x69,0x16,0x01,0x01,0x00,0x02,0x00,0x10}, state));
    CHECK(state.noiseControl.mode == NoiseControlMode::NoiseCancelling);
    CHECK(dispatcher.parseNotificationPayload({0x69,0x16,0x01,0x01,0x01,0x02,0x01,0x0b}, state));
    CHECK(state.noiseControl.mode == NoiseControlMode::Ambient);
    CHECK(state.noiseControl.ambientLevel == 11);
    CHECK(state.noiseControl.focusOnVoice);
    CHECK(dispatcher.parseNotificationPayload({0x69,0x16,0x01,0x00,0x00,0x02,0x00,0x0b}, state));
    CHECK(state.noiseControl.mode == NoiseControlMode::Off);
    CHECK_FALSE(dispatcher.parseNotificationPayload({0x69,0x16,0x01,0x01,0x01,0x00,0x01,0x0b}, state));
}

TEST_CASE("WH-XB910N DSEE uses subtype 0x00, default V2 unaffected", "[protocol][whxb910n]") {
    ReplyingFakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV2 p(session, false, NoiseControlLayout::WhXb910N, false, 0x00);
    SECTION("query") {
        fake.queueReply({
            SonyFrame{.type=DataType::Ack, .sequence=0},
            SonyFrame{.type=DataType::DataMdr, .sequence=1, .payload={0xe7,0x00,0x01}}
        });
        CHECK(p.getDsee());
        CHECK(FrameCodec::decode(fake.sentFrames().front()).payload == std::vector<uint8_t>{0xe6,0x00});
    }
    SECTION("set") {
        fake.queueReply({SonyFrame{.type=DataType::Ack, .sequence=0}});
        p.setDsee(false);
        CHECK(FrameCodec::decode(fake.lastSentFrame()).payload == std::vector<uint8_t>{0xe8,0x00,0x00});
    }
}

TEST_CASE("WH-XB910N EQ readback is 5 bands + Clear Bass on subtype 0x00", "[protocol][whxb910n]") {
    ReplyingFakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    ProtocolV2 p(session, false, NoiseControlLayout::WhXb910N);
    fake.queueReply({
        SonyFrame{.type=DataType::Ack, .sequence=0},
        SonyFrame{.type=DataType::DataMdr, .sequence=1,
                  .payload={0x57,0x00,0xa1,0x06,0x14,0x0d,0x0c,0x0c,0x0d,0x0e}}
    });
    const auto eq = p.getEqualizer();
    CHECK(FrameCodec::decode(fake.sentFrames().front()).payload == std::vector<uint8_t>{0x56,0x00});
    CHECK(eq.preset == 0xa1);
    CHECK(eq.clearBass == 10);
    CHECK(eq.bands == std::vector<int>{3,2,2,3,4});
}
