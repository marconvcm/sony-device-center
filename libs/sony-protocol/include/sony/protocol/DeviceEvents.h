#pragma once

#include "SemanticTypes.h"
#include "DeviceState.h"
#include <string>

namespace sony::protocol {

struct BatteryChanged {
    BatteryState battery;
};

struct NoiseControlChanged {
    NoiseControlState noiseControl;
};

struct EqualizerChanged {
    EqualizerState equalizer;
};

struct ConnectionChanged {
    bool connected{false};
    std::string deviceAddress;
};

struct DeviceStateChanged {
    DeviceStateSnapshot state;
};

} // namespace sony::protocol
