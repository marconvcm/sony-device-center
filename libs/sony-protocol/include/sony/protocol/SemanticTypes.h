#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

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
    std::array<int, 5> bands{0, 0, 0, 0, 0};
};

} // namespace sony::protocol
