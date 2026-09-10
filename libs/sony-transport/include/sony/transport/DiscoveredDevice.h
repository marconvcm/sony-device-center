#pragma once

#include "DeviceAddress.h"
#include <string>

namespace sony::transport {

struct DiscoveredDevice {
    std::string name;
    DeviceAddress address;

    bool operator==(const DiscoveredDevice& other) const = default;
};

} // namespace sony::transport
