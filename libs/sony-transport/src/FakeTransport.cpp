#include "sony/transport/FakeTransport.h"
#include <algorithm>

namespace sony::transport {

FakeTransport::FakeTransport() = default;

void FakeTransport::connect(const DeviceAddress& address) {
    std::lock_guard lock(_mutex);
    if (_failConnect) {
        throw SonyException(_failConnectCode, "Simulated connection failure");
    }
    _connected = true;
    _connectedAddress = address;
}

void FakeTransport::disconnect() noexcept {
    std::lock_guard lock(_mutex);
    _connected = false;
}

bool FakeTransport::isConnected() const noexcept {
    std::lock_guard lock(_mutex);
    return _connected;
}

size_t FakeTransport::send(std::span<const std::byte> data) {
    std::lock_guard lock(_mutex);
    if (!_connected) {
        throw SonyException(SonyErrorCode::Disconnected, "Transport is not connected");
    }
    if (_simulateTimeoutOnSend) {
        if (_timeoutSendCount > 0) {
            --_timeoutSendCount;
            if (_timeoutSendCount == 0) {
                _simulateTimeoutOnSend = false;
            }
        }
        throw SonyException(SonyErrorCode::Timeout, "Simulated send timeout");
    }

    std::vector<uint8_t> frame;
    frame.reserve(data.size());
    for (const auto b : data) {
        frame.push_back(static_cast<uint8_t>(b));
    }
    _sentFrames.push_back(std::move(frame));
    return data.size();
}

size_t FakeTransport::receive(std::span<std::byte> buffer) {
    std::lock_guard lock(_mutex);
    if (!_connected) {
        throw SonyException(SonyErrorCode::Disconnected, "Transport is not connected");
    }
    if (_simulateTimeoutOnReceive) {
        if (_timeoutReceiveCount > 0) {
            --_timeoutReceiveCount;
            if (_timeoutReceiveCount == 0) {
                _simulateTimeoutOnReceive = false;
            }
        }
        throw SonyException(SonyErrorCode::Timeout, "Simulated receive timeout");
    }
    if (_incomingQueue.empty()) {
        throw SonyException(SonyErrorCode::Timeout, "No data available in receive queue");
    }

    size_t toRead = std::min(buffer.size(), _incomingQueue.size());
    if (_maxReceiveChunkSize.has_value() && *_maxReceiveChunkSize > 0) {
        toRead = std::min(toRead, *_maxReceiveChunkSize);
    }

    for (size_t i = 0; i < toRead; ++i) {
        buffer[i] = static_cast<std::byte>(_incomingQueue.front());
        _incomingQueue.pop_front();
    }
    return toRead;
}

void FakeTransport::queueIncoming(std::span<const uint8_t> bytes) {
    std::lock_guard lock(_mutex);
    for (const auto b : bytes) {
        _incomingQueue.push_back(b);
    }
}

void FakeTransport::queueIncoming(std::span<const std::byte> bytes) {
    std::lock_guard lock(_mutex);
    for (const auto b : bytes) {
        _incomingQueue.push_back(static_cast<uint8_t>(b));
    }
}

void FakeTransport::queueIncoming(const std::vector<uint8_t>& bytes) {
    queueIncoming(std::span<const uint8_t>(bytes.data(), bytes.size()));
}

void FakeTransport::queueIncoming(const std::vector<std::byte>& bytes) {
    queueIncoming(std::span<const std::byte>(bytes.data(), bytes.size()));
}

void FakeTransport::queueIncoming(std::initializer_list<uint8_t> bytes) {
    std::lock_guard lock(_mutex);
    for (const auto b : bytes) {
        _incomingQueue.push_back(b);
    }
}

void FakeTransport::queueIncoming(const std::vector<std::vector<uint8_t>>& frames) {
    std::lock_guard lock(_mutex);
    for (const auto& frame : frames) {
        for (const auto b : frame) {
            _incomingQueue.push_back(b);
        }
    }
}

void FakeTransport::clearIncoming() {
    std::lock_guard lock(_mutex);
    _incomingQueue.clear();
}

size_t FakeTransport::incomingBytesAvailable() const noexcept {
    std::lock_guard lock(_mutex);
    return _incomingQueue.size();
}

std::vector<std::vector<uint8_t>> FakeTransport::sentFrames() const {
    std::lock_guard lock(_mutex);
    return _sentFrames;
}

std::vector<std::vector<std::byte>> FakeTransport::sentFramesBytes() const {
    std::lock_guard lock(_mutex);
    std::vector<std::vector<std::byte>> result;
    result.reserve(_sentFrames.size());
    for (const auto& frame : _sentFrames) {
        std::vector<std::byte> byteFrame;
        byteFrame.reserve(frame.size());
        for (const auto b : frame) {
            byteFrame.push_back(static_cast<std::byte>(b));
        }
        result.push_back(std::move(byteFrame));
    }
    return result;
}

size_t FakeTransport::sentCount() const noexcept {
    std::lock_guard lock(_mutex);
    return _sentFrames.size();
}

std::vector<uint8_t> FakeTransport::lastSentFrame() const {
    std::lock_guard lock(_mutex);
    if (_sentFrames.empty()) {
        return {};
    }
    return _sentFrames.back();
}

std::vector<uint8_t> FakeTransport::allSentBytes() const {
    std::lock_guard lock(_mutex);
    std::vector<uint8_t> result;
    for (const auto& frame : _sentFrames) {
        result.insert(result.end(), frame.begin(), frame.end());
    }
    return result;
}

void FakeTransport::clearSent() {
    std::lock_guard lock(_mutex);
    _sentFrames.clear();
}

void FakeTransport::setConnected(bool connected) noexcept {
    std::lock_guard lock(_mutex);
    _connected = connected;
}

void FakeTransport::simulateDisconnect() noexcept {
    disconnect();
}

void FakeTransport::setFailConnect(bool fail, SonyErrorCode code) {
    std::lock_guard lock(_mutex);
    _failConnect = fail;
    _failConnectCode = code;
}

void FakeTransport::simulateTimeoutOnReceive(bool enable, size_t count) {
    std::lock_guard lock(_mutex);
    _simulateTimeoutOnReceive = enable;
    _timeoutReceiveCount = count;
}

void FakeTransport::simulateTimeoutOnSend(bool enable, size_t count) {
    std::lock_guard lock(_mutex);
    _simulateTimeoutOnSend = enable;
    _timeoutSendCount = count;
}

void FakeTransport::setMaxReceiveChunkSize(std::optional<size_t> maxChunkSize) noexcept {
    std::lock_guard lock(_mutex);
    _maxReceiveChunkSize = maxChunkSize;
}

const DeviceAddress& FakeTransport::connectedAddress() const noexcept {
    std::lock_guard lock(_mutex);
    return _connectedAddress;
}

void FakeDeviceDiscovery::addDevice(DiscoveredDevice device) {
    std::lock_guard lock(_mutex);
    _devices.push_back(std::move(device));
}

void FakeDeviceDiscovery::setDevices(std::vector<DiscoveredDevice> devices) {
    std::lock_guard lock(_mutex);
    _devices = std::move(devices);
}

void FakeDeviceDiscovery::clear() {
    std::lock_guard lock(_mutex);
    _devices.clear();
}

std::vector<DiscoveredDevice> FakeDeviceDiscovery::discover() {
    std::lock_guard lock(_mutex);
    return _devices;
}

} // namespace sony::transport
