#include "sony/protocol/ProtocolV1.h"

namespace sony::protocol {

ProtocolV1::ProtocolV1(SonyProtocolSession& session)
    : _session(session) {}

void ProtocolV1::initDevice() {
    // V1 does not require an init handshake like V2; best-effort poll of ambient state
    try {
        _session.sendAndAwaitResponse(
            SonyFrame{ .type = DataType::DataMdr, .payload = {0x66, 0x02} },
            0x67,
            -1,
            std::chrono::milliseconds(500)
        );
    } catch (...) {}
}

BatteryState ProtocolV1::getBattery() {
    // CRITICAL REGRESSION SAFETY:
    // In V1, opcode 0x22 is POWER OFF. In V2, 0x22 is BATTERY GET.
    // Under NO circumstances may opcode 0x22 be sent to a V1 session here!
    // V1 devices report battery via unsolicited RFCOMM/HFP frames or notifications.
    std::lock_guard lock(_mutex);
    return _batteryState;
}

NoiseControlState ProtocolV1::getNoiseControl() {
    try {
        _session.sendAndAwaitResponse(
            SonyFrame{ .type = DataType::DataMdr, .payload = {0x66, 0x02} },
            0x67,
            -1,
            std::chrono::milliseconds(1000)
        );
    } catch (...) {}

    std::lock_guard lock(_mutex);
    return _noiseControlState;
}

void ProtocolV1::setNoiseControl(const NoiseControlState& state) {
    uint8_t effect = (state.mode == NoiseControlMode::Off) ? 0 : 17; // ADJUSTMENT_COMPLETION
    uint8_t ncSettingType = 1; // LEVEL_ADJUSTMENT
    uint8_t asmSettingType = 1; // LEVEL_ADJUSTMENT
    uint8_t asmId = state.focusOnVoice ? 1 : 0;
    int8_t asmLevel = (state.mode == NoiseControlMode::Ambient) ? static_cast<int8_t>(state.ambientLevel) : -1;
    uint8_t dualSingle = (asmLevel == -1) ? 0 : (asmLevel == 1 ? 1 : 2);

    std::vector<uint8_t> payload = {
        0x68, // NCASM_SET_PARAM (104)
        0x02, // NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE
        effect,
        ncSettingType,
        dualSingle,
        asmSettingType,
        asmId,
        static_cast<uint8_t>(asmLevel)
    };

    _session.send(SonyFrame{ .type = DataType::DataMdr, .payload = std::move(payload) });

    std::lock_guard lock(_mutex);
    _noiseControlState = state;
}

EqualizerState ProtocolV1::getEqualizer() {
    throw SonyException(SonyErrorCode::Unsupported, "Equalizer query is not supported on Protocol V1");
}

void ProtocolV1::setEqualizerPreset(int /*preset*/) {
    throw SonyException(SonyErrorCode::Unsupported, "Equalizer preset is not supported on Protocol V1");
}

void ProtocolV1::setEqualizerCustom(int /*clearBass*/, const std::array<int, 5>& /*bands*/) {
    throw SonyException(SonyErrorCode::Unsupported, "Custom equalizer is not supported on Protocol V1");
}

bool ProtocolV1::getDsee() {
    throw SonyException(SonyErrorCode::Unsupported, "DSEE is not supported on Protocol V1");
}

void ProtocolV1::setDsee(bool /*enabled*/) {
    throw SonyException(SonyErrorCode::Unsupported, "DSEE is not supported on Protocol V1");
}

std::string ProtocolV1::getFirmwareVersion() {
    throw SonyException(SonyErrorCode::Unsupported, "Firmware query is not supported on Protocol V1");
}

std::string ProtocolV1::getCodec() {
    throw SonyException(SonyErrorCode::Unsupported, "Codec query is not supported on Protocol V1");
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
