#pragma once

#include "IDeviceService.h"
#include "sony/transport/IDeviceDiscovery.h"
#include <mutex>

namespace sony::core {

class DeviceService : public IDeviceService {
public:
    explicit DeviceService(
        std::shared_ptr<transport::ITransport> transport,
        std::shared_ptr<transport::IDeviceDiscovery> discovery = nullptr);
    ~DeviceService() override;

    std::vector<DiscoveredDevice> discoverDevices() override;
    void connect(const transport::DeviceAddress& address, std::string_view name = "") override;
    void disconnect() noexcept override;
    [[nodiscard]] bool isConnected() const noexcept override;

    [[nodiscard]] SonyDevice* activeDevice() noexcept override;
    [[nodiscard]] protocol::DeviceStateSnapshot snapshot() const override;

private:
    std::shared_ptr<transport::ITransport> _transport;
    std::shared_ptr<transport::IDeviceDiscovery> _discovery;
    std::unique_ptr<SonyDevice> _device;
    mutable std::mutex _mutex;
};

} // namespace sony::core
