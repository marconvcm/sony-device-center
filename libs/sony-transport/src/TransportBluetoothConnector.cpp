#include "sony/transport/TransportBluetoothConnector.h"
#include "sony/transport/SonyError.h"

namespace sony::transport {

TransportBluetoothConnector::TransportBluetoothConnector(std::unique_ptr<ITransport> transport,
                                                         std::unique_ptr<IDeviceDiscovery> discovery,
                                                         SonyProtocolVersion version)
    : _ownedTransport(std::move(transport)),
      _transport(_ownedTransport.get()),
      _ownedDiscovery(std::move(discovery)),
      _discovery(_ownedDiscovery.get()),
      _version(version) {}

TransportBluetoothConnector::TransportBluetoothConnector(ITransport* transport,
                                                         IDeviceDiscovery* discovery,
                                                         SonyProtocolVersion version)
    : _transport(transport),
      _discovery(discovery),
      _version(version) {}

TransportBluetoothConnector::~TransportBluetoothConnector() = default;

int TransportBluetoothConnector::send(char* buf, size_t length) {
    if (!_transport) {
        throw RecoverableException("Transport is null", true);
    }
    try {
        auto span = std::span<const std::byte>(reinterpret_cast<const std::byte*>(buf), length);
        return static_cast<int>(_transport->send(span));
    } catch (const SonyException& e) {
        bool shouldDisconnect = (e.code() == SonyErrorCode::Disconnected);
        throw RecoverableException(e.what(), shouldDisconnect);
    } catch (const std::exception& e) {
        throw RecoverableException(e.what(), true);
    }
}

int TransportBluetoothConnector::recv(char* buf, size_t length) {
    if (!_transport) {
        throw RecoverableException("Transport is null", true);
    }
    try {
        auto span = std::span<std::byte>(reinterpret_cast<std::byte*>(buf), length);
        return static_cast<int>(_transport->receive(span));
    } catch (const SonyException& e) {
        bool shouldDisconnect = (e.code() == SonyErrorCode::Disconnected);
        throw RecoverableException(e.what(), shouldDisconnect);
    } catch (const std::exception& e) {
        throw RecoverableException(e.what(), true);
    }
}

void TransportBluetoothConnector::connect(const std::string& addrStr) {
    if (!_transport) {
        throw RecoverableException("Transport is null", true);
    }
    try {
        _transport->connect(DeviceAddress(addrStr));
    } catch (const SonyException& e) {
        throw RecoverableException(e.what(), true);
    } catch (const std::exception& e) {
        throw RecoverableException(e.what(), true);
    }
}

void TransportBluetoothConnector::disconnect() noexcept {
    if (_transport) {
        _transport->disconnect();
    }
}

bool TransportBluetoothConnector::isConnected() noexcept {
    return _transport && _transport->isConnected();
}

std::vector<BluetoothDevice> TransportBluetoothConnector::getConnectedDevices() {
    if (!_discovery) {
        return {};
    }
    auto discovered = _discovery->discover();
    std::vector<BluetoothDevice> result;
    result.reserve(discovered.size());
    for (const auto& d : discovered) {
        result.push_back(BluetoothDevice{
            .name = d.name,
            .mac = d.address.str()
        });
    }
    return result;
}

SonyProtocolVersion TransportBluetoothConnector::getProtocolVersion() noexcept {
    return _version;
}

void TransportBluetoothConnector::setProtocolVersion(SonyProtocolVersion version) noexcept {
    _version = version;
}

ITransport* TransportBluetoothConnector::transport() const noexcept {
    return _transport;
}

IDeviceDiscovery* TransportBluetoothConnector::discovery() const noexcept {
    return _discovery;
}

} // namespace sony::transport
