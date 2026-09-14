#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sony::protocol {

struct BatteryState {
    std::optional<int> main;
    std::optional<int> left;
    std::optional<int> right;
    std::optional<int> caseBattery;
    bool charging{false};
};

enum class NoiseControlMode {
    Off,
    NoiseCancelling,
    Ambient
};

struct NoiseControlState {
    NoiseControlMode mode{NoiseControlMode::Off};
    int ambientLevel{0};
    bool focusOnVoice{false};
};

struct EqualizerState {
    int preset{0};
    int clearBass{0};
    // 5 elements (+ clearBass) on legacy devices; 10 elements, no separate
    // clearBass, on newer devices such as the WH-1000XM6 -- see
    // DeviceCapabilities::tenBandEqualizer.
    std::vector<int> bands{0, 0, 0, 0, 0};
};

} // namespace sony::protocol
