#pragma once

#include "IBluetoothConnector.h"
#include "ITransport.h"
#include "IDeviceDiscovery.h"

#include <memory>

namespace sony::transport {

class TransportBluetoothConnector : public IBluetoothConnector {
public:
    explicit TransportBluetoothConnector(std::unique_ptr<ITransport> transport,
                                         std::unique_ptr<IDeviceDiscovery> discovery = nullptr,
                                         SonyProtocolVersion version = SonyProtocolVersion::V2);
    explicit TransportBluetoothConnector(ITransport* transport,
                                         IDeviceDiscovery* discovery = nullptr,
                                         SonyProtocolVersion version = SonyProtocolVersion::V2);
    ~TransportBluetoothConnector() override;

    int send(char* buf, size_t length) override;
    int recv(char* buf, size_t length) override;
    void connect(const std::string& addrStr) override;
    void disconnect() noexcept override;
    bool isConnected() noexcept override;
    std::vector<BluetoothDevice> getConnectedDevices() override;
    SonyProtocolVersion getProtocolVersion() noexcept override;

    void setProtocolVersion(SonyProtocolVersion version) noexcept;
    [[nodiscard]] ITransport* transport() const noexcept;
    [[nodiscard]] IDeviceDiscovery* discovery() const noexcept;

private:
    std::unique_ptr<ITransport> _ownedTransport;
    ITransport* _transport{nullptr};

    std::unique_ptr<IDeviceDiscovery> _ownedDiscovery;
    IDeviceDiscovery* _discovery{nullptr};

    SonyProtocolVersion _version{SonyProtocolVersion::V2};
};

} // namespace sony::transport
