#pragma once

#include "DiscoveredDevice.h"
#include <vector>

namespace sony::transport {

class IDeviceDiscovery {
public:
    virtual ~IDeviceDiscovery() = default;
    virtual std::vector<DiscoveredDevice> discover() = 0;
};

} // namespace sony::transport
