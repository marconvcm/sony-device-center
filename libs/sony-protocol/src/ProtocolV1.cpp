#include "sony/protocol/ProtocolV1.h"
#include "ProtocolHelpers.h"

#include <algorithm>
#include <chrono>

// Byte layouts below match Client/CommandSerializer.cpp (the legacy client,
// exercised on WH-1000XM3 hardware for years) and Gadgetbridge's
// SonyProtocolImplV1. Every GET was additionally replayed against a
// WH-1000XM4 on firmware 3.0.1 before being trusted here.
//
// CRITICAL: opcode 0x22 is POWER OFF on this generation. It must never be
// sent from this file; ProtocolV1Tests pins that for the battery path.

namespace sony::protocol {

using detail::clampEqValue;
using detail::codecName;

namespace {

constexpr uint8_t kNcAsmInquired = 0x02;  // NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE
constexpr uint8_t kEqInquired = 0x01;     // PRESET_EQ

constexpr uint8_t kEffectOff = 0x00;
constexpr uint8_t kEffectAdjustmentCompletion = 0x11;
constexpr uint8_t kLevelAdjustment = 0x01;
constexpr uint8_t kDualSingleOff = 0x00;   // ambient sound passthrough
constexpr uint8_t kDualSingleDual = 0x02;  // noise cancelling

constexpr auto kTimeout = std::chrono::milliseconds(1000);

} // namespace

ProtocolV1::ProtocolV1(SonyProtocolSession& session)
    : _session(session) {}

void ProtocolV1::initDevice() {
    // V1 does not require an init handshake like V2; best-effort poll of
    // ambient state. The first reply after connecting is the slow one on a
    // WH-1000XM4 (well past 500 ms), and giving up early leaves its late ACK
    // and data frame to collide with the next request, so wait as long as
    // any other query does.
    try {
        _session.sendAndAwaitResponse(
            SonyFrame{ .type = DataType::DataMdr, .payload = {0x66, kNcAsmInquired} },
            0x67,
            kNcAsmInquired,
            std::chrono::milliseconds(1500)
        );
    } catch (const SonyException&) {}
}

BatteryState ProtocolV1::getBattery() {
    // GET 10 <type> -> RET 11 <type> ..., type 0x00 single, 0x01 dual (L/R),
    // 0x02 case. Over-ear models answer every type and echo the single reading
    // into the left slot, so the single reading is authoritative when it
    // arrives and dual/case are only consulted for earbuds.
    BatteryState state;

    // 1. Single battery: GET 10 00 -> RET 11 00 <level> <charging>
    try {
        auto resp = _session.sendAndAwaitResponse(
            SonyFrame{ .type = DataType::DataMdr, .payload = {0x10, 0x00} },
            0x11, 0x00, kTimeout);
        if (resp.payload.size() >= 4) {
            state.main = static_cast<int>(resp.payload[2]);
            state.charging = (resp.payload[3] == 1);
            return state;
        }
    } catch (const SonyException&) {}

    // 2. Dual L/R battery: GET 10 01 -> RET 11 01 <Llvl> <Lchg> <Rlvl> <Rchg>
    try {
        auto resp = _session.sendAndAwaitResponse(
            SonyFrame{ .type = DataType::DataMdr, .payload = {0x10, 0x01} },
            0x11, 0x01, kTimeout);
        if (resp.payload.size() >= 6) {
            const int left = static_cast<int>(resp.payload[2]);
            const int right = static_cast<int>(resp.payload[4]);
            state.charging = (resp.payload[3] == 1 || resp.payload[5] == 1);
            if (right == 0 && left > 0) {
                // Over-ear models answer the dual query too, echoing their
                // single cell into the left slot with an empty right one.
                // Reporting min(left, right) there would read as 0%.
                state.main = left;
            } else {
                state.left = left;
                state.right = right;
                state.main = std::min(left, right);
            }
        }
    } catch (const SonyException&) {}

    // 3. Case battery: GET 10 02 -> RET 11 02 <level> <charging>
    try {
        auto resp = _session.sendAndAwaitResponse(
            SonyFrame{ .type = DataType::DataMdr, .payload = {0x10, 0x02} },
            0x11, 0x02, kTimeout);
        if (resp.payload.size() >= 4) {
            state.caseBattery = static_cast<int>(resp.payload[2]);
        }
    } catch (const SonyException&) {}

    return state;
}

NoiseControlState ProtocolV1::getNoiseControl() {
    // GET 66 02 -> RET 67 02 <effect> <ncSettingType> <dualSingle> <asmSettingType> <asmId> <asmLevel>
    // effect 0 is off. Otherwise dualSingle selects the mode: 0 is ambient
    // sound at <asmLevel>, 1 (single) and 2 (dual) are noise cancelling.
    auto resp = _session.sendAndAwaitResponse(
        SonyFrame{ .type = DataType::DataMdr, .payload = {0x66, kNcAsmInquired} },
        0x67, kNcAsmInquired, kTimeout);

    if (resp.payload.size() < 8 || resp.payload[1] != kNcAsmInquired)
        throw SonyException(SonyErrorCode::InvalidResponse, "Malformed noise-control response");

    const bool on = resp.payload[2] != kEffectOff;
    const bool ambient = resp.payload[4] == kDualSingleOff;
    const bool voice = resp.payload[6] == 1;
    const int level = static_cast<int>(resp.payload[7]);

    NoiseControlState state;
    if (!on) {
        state.mode = NoiseControlMode::Off;
    } else if (ambient) {
        state.mode = NoiseControlMode::Ambient;
    } else {
        state.mode = NoiseControlMode::NoiseCancelling;
    }
    state.ambientLevel = (state.mode == NoiseControlMode::Ambient) ? level : 0;
    state.focusOnVoice = voice;
    return state;
}

void ProtocolV1::setNoiseControl(const NoiseControlState& state) {
    // SET 68 02 <effect> 01 <dualSingle> 01 <asmId> <asmLevel>
    // Noise cancelling is dual NC with the level at 0; ambient sound is
    // dual/single OFF with the level in the last byte. This is what the legacy
    // client has always sent and what the headset echoes back on GET.
    const bool off = state.mode == NoiseControlMode::Off;
    const bool ambient = state.mode == NoiseControlMode::Ambient;
    const uint8_t level = ambient ? static_cast<uint8_t>(std::clamp(state.ambientLevel, 1, 20)) : 0;

    std::vector<uint8_t> payload = {
        0x68,
        kNcAsmInquired,
        off ? kEffectOff : kEffectAdjustmentCompletion,
        kLevelAdjustment,
        (ambient || off) ? kDualSingleOff : kDualSingleDual,
        kLevelAdjustment,
        static_cast<uint8_t>(state.focusOnVoice ? 1 : 0),
        level
    };

    _session.send(SonyFrame{ .type = DataType::DataMdr, .payload = std::move(payload) });
}

EqualizerState ProtocolV1::getEqualizer() {
    // GET 56 01 -> RET 57 01 <preset> 06 <bass+10> <b1..b5 +10>
    // Same table as V2, behind inquired type 0x01 (PRESET_EQ) instead of 0x00.
    auto resp = _session.sendAndAwaitResponse(
        SonyFrame{ .type = DataType::DataMdr, .payload = {0x56, kEqInquired} },
        0x57, kEqInquired, kTimeout);

    if (resp.payload.size() < 10 || resp.payload[1] != kEqInquired)
        throw SonyException(SonyErrorCode::InvalidResponse, "Incomplete equalizer response");

    EqualizerState state;
    state.preset = static_cast<int>(resp.payload[2]);
    state.clearBass = static_cast<int>(resp.payload[4]) - 10;
    for (size_t i = 0; i < 5; ++i) {
        state.bands[i] = static_cast<int>(resp.payload[5 + i]) - 10;
    }
    return state;
}

void ProtocolV1::setEqualizerPreset(int preset) {
    // SET preset: 58 01 <preset> 00
    std::vector<uint8_t> payload = {
        0x58,
        kEqInquired,
        static_cast<uint8_t>(preset),
        0x00
    };
    _session.send(SonyFrame{ .type = DataType::DataMdr, .payload = std::move(payload) });
}

void ProtocolV1::setEqualizerCustom(int clearBass, const std::array<int, 5>& bands) {
    // SET custom: 58 01 A0 06 <clearBass+10> <b1..b5 +10>
    std::vector<uint8_t> payload = {
        0x58,
        kEqInquired,
        0xa0,
        0x06,
        clampEqValue(clearBass)
    };
    for (int b : bands) {
        payload.push_back(clampEqValue(b));
    }
    _session.send(SonyFrame{ .type = DataType::DataMdr, .payload = std::move(payload) });
}

bool ProtocolV1::getDsee() {
    throw SonyException(SonyErrorCode::Unsupported, "DSEE is not supported on Protocol V1");
}

void ProtocolV1::setDsee(bool /*enabled*/) {
    throw SonyException(SonyErrorCode::Unsupported, "DSEE is not supported on Protocol V1");
}

std::string ProtocolV1::getFirmwareVersion() {
    // GET 04 02 -> RET 05 02 <len> <ascii version...>
    auto resp = _session.sendAndAwaitResponse(
        SonyFrame{ .type = DataType::DataMdr, .payload = {0x04, 0x02} },
        0x05, -1, kTimeout);
    if (resp.payload.size() > 3) {
        return std::string(resp.payload.begin() + 3, resp.payload.end());
    }
    return "";
}

std::string ProtocolV1::getCodec() {
    // GET 18 00 -> RET 19 00 <codec>
    auto resp = _session.sendAndAwaitResponse(
        SonyFrame{ .type = DataType::DataMdr, .payload = {0x18, 0x00} },
        0x19, -1, kTimeout);
    if (resp.payload.size() >= 3) {
        return codecName(resp.payload[2]);
    }
    return "";
}

int ProtocolV1::getAutoPowerOff() {
    throw SonyException(SonyErrorCode::Unsupported, "Auto Power Off is not supported on Protocol V1");
}

void ProtocolV1::setAutoPowerOff(int /*index*/) {
    throw SonyException(SonyErrorCode::Unsupported, "Auto Power Off is not supported on Protocol V1");
}

bool ProtocolV1::getSpeakToChat() {
    throw SonyException(SonyErrorCode::Unsupported, "Speak-to-Chat is not supported on Protocol V1");
}

void ProtocolV1::setSpeakToChat(bool /*enabled*/) {
    throw SonyException(SonyErrorCode::Unsupported, "Speak-to-Chat is not supported on Protocol V1");
}

bool ProtocolV1::getAdaptiveVolume() {
    throw SonyException(SonyErrorCode::Unsupported, "Adaptive Volume is not supported on Protocol V1");
}

void ProtocolV1::setAdaptiveVolume(bool /*enabled*/) {
    throw SonyException(SonyErrorCode::Unsupported, "Adaptive Volume is not supported on Protocol V1");
}

void ProtocolV1::setVpt(int preset) {
    // VPT_SET_PARAM (72), VPT (1), preset
    std::vector<uint8_t> payload = {
        0x48,
        0x01,
        static_cast<uint8_t>(preset)
    };
    _session.send(SonyFrame{ .type = DataType::DataMdr, .payload = std::move(payload) });
}

void ProtocolV1::setSoundPosition(int preset) {
    // VPT_SET_PARAM (72), SOUND_POSITION (2), preset
    std::vector<uint8_t> payload = {
        0x48,
        0x02,
        static_cast<uint8_t>(preset)
    };
    _session.send(SonyFrame{ .type = DataType::DataMdr, .payload = std::move(payload) });
}

} // namespace sony::protocol
