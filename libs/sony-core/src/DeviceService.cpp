#include "sony/core/DeviceService.h"
#include "sony/protocol/DeviceProfileRegistry.h"

namespace sony::core {

DeviceService::DeviceService(
    std::shared_ptr<transport::ITransport> transport,
    std::shared_ptr<transport::IDeviceDiscovery> discovery)
    : _transport(std::move(transport)), _discovery(std::move(discovery)) {}

DeviceService::~DeviceService() {
    disconnect();
}

std::vector<DiscoveredDevice> DeviceService::discoverDevices() {
    if (!_discovery) {
        return {};
    }
    auto rawDevices = _discovery->discover();
    std::vector<DiscoveredDevice> result;
    result.reserve(rawDevices.size());
    for (const auto& dev : rawDevices) {
        auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(dev.name);
        result.push_back(DiscoveredDevice{
            .address = dev.address.str(),
            .name = dev.name,
            .version = profile.protocol
        });
    }
    return result;
}

void DeviceService::connect(const transport::DeviceAddress& address, std::string_view name) {
    std::lock_guard lock(_mutex);
    if (!_device) {
        // The version passed here is provisional; SonyDevice::connect() resolves
        // it from the device profile. Start from V1 so that a failure to resolve
        // can never leave a legacy device on the V2 command set.
        _device = std::make_unique<SonyDevice>(_transport, SonyProtocolVersion::V1);
    }
    _device->connect(address, name);
}

void DeviceService::disconnect() noexcept {
    std::lock_guard lock(_mutex);
    if (_device) {
        _device->disconnect();
    }
}

bool DeviceService::isConnected() const noexcept {
    std::lock_guard lock(_mutex);
    return _device && _device->isConnected();
}

SonyDevice* DeviceService::activeDevice() noexcept {
    std::lock_guard lock(_mutex);
    return _device.get();
}

protocol::DeviceStateSnapshot DeviceService::snapshot() const {
    std::lock_guard lock(_mutex);
    if (_device) {
        return _device->snapshot();
    }
    return std::make_shared<const protocol::DeviceState>();
}

} // namespace sony::core
