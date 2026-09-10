#pragma once

#include "SemanticTypes.h"
#include <memory>

namespace sony::protocol {

struct DeviceState {
    BatteryState battery;
    NoiseControlState noiseControl;
    EqualizerState equalizer;

    bool dsee{false};

    std::string firmware;
    std::string codec;

    int autoPowerOff{0};
    bool speakToChat{false};
    bool adaptiveVolume{false};
};

using DeviceStateSnapshot = std::shared_ptr<const DeviceState>;

} // namespace sony::protocol
