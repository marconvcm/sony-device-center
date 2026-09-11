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
    SonyProtocolVersion version{SonyProtocolVersion::V1};
    std::optional<bool> paired;
    std::optional<bool> connected;
};

class IDeviceService {
public:
    virtual ~IDeviceService() = default;

    virtual void tick() {}
    virtual void startAutoConnect(std::string address = {}) {}
    virtual std::string connectionState() const { return isConnected() ? "connected" : "disconnected"; }
    virtual std::string selectedAddress() const { return {}; }
    virtual std::string lastError() const { return {}; }
    virtual std::vector<DiscoveredDevice> discoverDevices() = 0;
    virtual void connect(const transport::DeviceAddress& address, std::string_view name = "") = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    [[nodiscard]] virtual SonyDevice* activeDevice() noexcept = 0;
    [[nodiscard]] virtual protocol::DeviceStateSnapshot snapshot() const = 0;
};

} // namespace sony::core
