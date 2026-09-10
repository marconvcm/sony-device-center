#include "sony/transport/BluetoothConnectorTransport.h"
#include "IBluetoothConnector.h"

namespace sony::transport {

BluetoothConnectorTransport::BluetoothConnectorTransport(std::unique_ptr<IBluetoothConnector> connector)
    : _ownedConnector(std::move(connector)), _connector(_ownedConnector.get()) {}

BluetoothConnectorTransport::BluetoothConnectorTransport(IBluetoothConnector* connector)
    : _connector(connector) {}

BluetoothConnectorTransport::~BluetoothConnectorTransport() = default;

void BluetoothConnectorTransport::connect(const DeviceAddress& address) {
    if (!_connector) {
        throw SonyException(SonyErrorCode::TransportFailure, "No underlying connector configured");
    }
    try {
        _connector->connect(address.str());
    } catch (const RecoverableException& e) {
        throw SonyException(SonyErrorCode::TransportFailure, e.what());
    } catch (const std::exception& e) {
        throw SonyException(SonyErrorCode::TransportFailure, e.what());
    }
}

void BluetoothConnectorTransport::disconnect() noexcept {
    if (_connector) {
        _connector->disconnect();
    }
}

bool BluetoothConnectorTransport::isConnected() const noexcept {
    return _connector && _connector->isConnected();
}

size_t BluetoothConnectorTransport::send(std::span<const std::byte> data) {
    if (!_connector || !_connector->isConnected()) {
        throw SonyException(SonyErrorCode::Disconnected, "Transport not connected");
    }
    try {
        int sent = _connector->send(reinterpret_cast<char*>(const_cast<std::byte*>(data.data())), data.size());
        if (sent < 0) {
            throw SonyException(SonyErrorCode::TransportFailure, "Failed to send data");
        }
        return static_cast<size_t>(sent);
    } catch (const SonyException&) {
        throw;
    } catch (const RecoverableException& e) {
        if (e.shouldDisconnect) {
            throw SonyException(SonyErrorCode::Disconnected, e.what());
        }
        throw SonyException(SonyErrorCode::TransportFailure, e.what());
    } catch (const std::exception& e) {
        throw SonyException(SonyErrorCode::TransportFailure, e.what());
    }
}

size_t BluetoothConnectorTransport::receive(std::span<std::byte> buffer) {
    if (!_connector || !_connector->isConnected()) {
        throw SonyException(SonyErrorCode::Disconnected, "Transport not connected");
    }
    try {
        int bytesRead = _connector->recv(reinterpret_cast<char*>(buffer.data()), buffer.size());
        if (bytesRead <= 0) {
            throw SonyException(SonyErrorCode::Disconnected, "Transport closed or receive error");
        }
        return static_cast<size_t>(bytesRead);
    } catch (const SonyException&) {
        throw;
    } catch (const RecoverableException& e) {
        if (e.shouldDisconnect) {
            throw SonyException(SonyErrorCode::Disconnected, e.what());
        }
        throw SonyException(SonyErrorCode::Timeout, e.what());
    } catch (const std::exception& e) {
        throw SonyException(SonyErrorCode::TransportFailure, e.what());
    }
}

IBluetoothConnector* BluetoothConnectorTransport::connector() const noexcept {
    return _connector;
}

BluetoothConnectorDiscovery::BluetoothConnectorDiscovery(std::unique_ptr<IBluetoothConnector> connector)
    : _ownedConnector(std::move(connector)), _connector(_ownedConnector.get()) {}

BluetoothConnectorDiscovery::BluetoothConnectorDiscovery(IBluetoothConnector* connector)
    : _connector(connector) {}

std::vector<DiscoveredDevice> BluetoothConnectorDiscovery::discover() {
    if (!_connector) {
        return {};
    }
    auto bldevs = _connector->getConnectedDevices();
    std::vector<DiscoveredDevice> result;
    result.reserve(bldevs.size());
    for (const auto& dev : bldevs) {
        result.push_back(DiscoveredDevice{
            .name = dev.name,
            .address = DeviceAddress(dev.mac)
        });
    }
    return result;
}

} // namespace sony::transport
