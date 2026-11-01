#pragma once
#include "sony/protocol/DeviceState.h"
#include <optional>
#include <span>

namespace sony::protocol::detail {
// Shared decoding keeps GET replies and unsolicited notifications consistent.
inline std::optional<NoiseControlState> decodeNoiseControl(std::span<const uint8_t> p) {
    if (p.size() < 7 || (p[0] != 0x67 && p[0] != 0x69) || p[2] != 1) return {};
    size_t voice = 5, level = 6;
    if (p[1] == 0x15) {
        if (p.size() != 8) return {};
        voice = 6; level = 7; // byte 5 is wind control, not voice focus
        if (p[3] > 1 || p[4] > 1 || p[voice] > 1 || p[level] > 20) return {};
    } else if (p[1] != 0x17 && p[1] != 0x19) return {};
    NoiseControlState result;
    result.mode = p[3] ? (p[4] ? NoiseControlMode::Ambient : NoiseControlMode::NoiseCancelling) : NoiseControlMode::Off;
    result.ambientLevel = p[4] ? p[level] : 0;
    result.focusOnVoice = p[voice] != 0;
    return result;
}
}
