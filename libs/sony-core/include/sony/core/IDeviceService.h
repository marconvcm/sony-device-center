#pragma once

#include "SonyDevice.h"
#include "sony/transport/ITransport.h"

#include <memory>
#include <string>
#include <vector>

namespace sony::core {

struct DiscoveredDevice {
    std::string address;
    std::string name;
    SonyProtocolVersion version{SonyProtocolVersion::V2};
};

class IDeviceService {
public:
    virtual ~IDeviceService() = default;

    virtual std::vector<DiscoveredDevice> discoverDevices() = 0;
    virtual void connect(const transport::DeviceAddress& address, std::string_view name = "") = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    [[nodiscard]] virtual SonyDevice* activeDevice() noexcept = 0;
    [[nodiscard]] virtual protocol::DeviceStateSnapshot snapshot() const = 0;
};

} // namespace sony::core
