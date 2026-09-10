#pragma once

#include "CapabilityCache.h"
#include "DeviceProfileRegistry.h"
#include "IProtocol.h"

#include <future>
#include <memory>
#include <string_view>

namespace sony::protocol {

class CapabilityDiscovery {
public:
    explicit CapabilityDiscovery(std::shared_ptr<CapabilityCache> cache = nullptr);

    // Fast synchronous discovery: returns immediately for known devices or cached profiles;
    // performs probing only for unknown, uncached devices.
    DeviceCapabilities discover(
        IProtocol& protocol,
        std::string_view deviceName = "",
        std::string_view address = "");

    // Asynchronous capability discovery: never blocks the calling or UI thread.
    std::future<DeviceCapabilities> discoverAsync(
        IProtocol& protocol,
        std::string_view deviceName = "",
        std::string_view address = "");

    // Directly probe an unknown device feature by feature
    DeviceCapabilities probeDevice(
        IProtocol& protocol,
        SonyModel model = SonyModel::Unknown,
        std::string_view address = "");

    [[nodiscard]] std::shared_ptr<CapabilityCache> cache() const noexcept {
        return _cache;
    }

private:
    std::shared_ptr<CapabilityCache> _cache;
};

} // namespace sony::protocol
