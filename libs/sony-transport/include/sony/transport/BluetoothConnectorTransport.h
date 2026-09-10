#pragma once

#include "ITransport.h"
#include "IDeviceDiscovery.h"
#include "SonyError.h"

#include <memory>

class IBluetoothConnector;

namespace sony::transport {

class BluetoothConnectorTransport : public ITransport {
public:
    explicit BluetoothConnectorTransport(std::unique_ptr<IBluetoothConnector> connector);
    explicit BluetoothConnectorTransport(IBluetoothConnector* connector);
    ~BluetoothConnectorTransport() override;

    void connect(const DeviceAddress& address) override;
    void disconnect() noexcept override;
    [[nodiscard]] bool isConnected() const noexcept override;
    size_t send(std::span<const std::byte> data) override;
    size_t receive(std::span<std::byte> buffer) override;

    [[nodiscard]] IBluetoothConnector* connector() const noexcept;

private:
    std::unique_ptr<IBluetoothConnector> _ownedConnector;
    IBluetoothConnector* _connector{nullptr};
};

class BluetoothConnectorDiscovery : public IDeviceDiscovery {
public:
    explicit BluetoothConnectorDiscovery(std::unique_ptr<IBluetoothConnector> connector);
    explicit BluetoothConnectorDiscovery(IBluetoothConnector* connector);
    ~BluetoothConnectorDiscovery() override = default;

    std::vector<DiscoveredDevice> discover() override;

private:
    std::unique_ptr<IBluetoothConnector> _ownedConnector;
    IBluetoothConnector* _connector{nullptr};
};

} // namespace sony::transport
