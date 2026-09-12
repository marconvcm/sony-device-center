#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

// Byte-level helpers shared by the V1 and V2 command sets. Both generations
// encode equalizer values as value+10 and report the same codec codes.
namespace sony::protocol::detail {

inline std::string codecName(uint8_t code) {
    switch (code) {
        case 0x01: return "SBC";
        case 0x02: return "AAC";
        case 0x10: return "LDAC";
        case 0x20: return "aptX";
        case 0x21: return "aptX HD";
        default:   return "";
    }
}

inline uint8_t clampEqValue(int v) {
    return static_cast<uint8_t>(std::max(-10, std::min(10, v)) + 10);
}

// The newer 10-band layout (e.g. WH-1000XM6) encodes each band as a raw,
// unsigned device value with no +10 bias -- a real capture of Sound Connect
// showed the range 0x00..0x0c (0..12). Sound Connect's own dB labels for
// this scale have not been reverse-engineered, so callers see/set these as
// opaque raw units rather than a (possibly wrong) dB conversion.
inline uint8_t clampEqValueRaw(int v) {
    return static_cast<uint8_t>(std::max(0, std::min(12, v)));
}

} // namespace sony::protocol::detail
