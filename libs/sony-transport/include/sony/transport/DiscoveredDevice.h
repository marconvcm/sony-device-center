#pragma once
#include <optional>

#include "DeviceAddress.h"
#include <string>

namespace sony::transport {

struct DiscoveredDevice {
    std::string name;
    DeviceAddress address;
    std::optional<bool> paired;
    std::optional<bool> connected;

    bool operator==(const DiscoveredDevice& other) const = default;
};

} // namespace sony::transport
