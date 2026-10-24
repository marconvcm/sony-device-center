#include "sony/transport/LinuxBleTransport.h"
#include "sony/transport/Logger.h"
#include "sony/transport/SonyError.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

namespace sony::transport {
namespace {
void closeFd(int& fd) { if (fd >= 0) ::close(fd); fd = -1; }
void nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw SonyException(SonyErrorCode::TransportFailure, "Cannot configure BLE descriptor");
}
}

LinuxBleTransport::LinuxBleTransport(std::unique_ptr<ITransport> classic, std::unique_ptr<IBluezGattClient> client)
    : _classic(std::move(classic)), _client(std::move(client)) {
    _cancelFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (_cancelFd < 0) throw SonyException(SonyErrorCode::TransportFailure, "Cannot create BLE cancellation descriptor");
}
LinuxBleTransport::~LinuxBleTransport() { disconnect(); closeFd(_cancelFd); }

void LinuxBleTransport::connect(const DeviceAddress& address) {
    disconnect();
    _ble = false;
    uint64_t ignored;
    while (::read(_cancelFd, &ignored, sizeof(ignored)) > 0) {}
    try {
        auto endpoint = _client->resolve(address);
        if (!endpoint) { _client->release(); _classic->connect(address); return; }
        _ble = true;
        auto rx = _client->acquire(endpoint->notifyPath, true);
        _readFd = rx.fd;
        auto tx = _client->acquire(endpoint->writePath, false);
        _writeFd = tx.fd;
        if (rx.mtu < 23 || tx.mtu < 23 || rx.mtu > 65535 || tx.mtu > 65535)
            throw SonyException(SonyErrorCode::InvalidResponse, "Invalid BLE MTU");
        _writeMtu = tx.mtu - 3;
        nonblocking(_readFd); nonblocking(_writeFd);
        _connected = true;
        Logger::info(LogCategory::Transport, "Sony BLE control connected; existing audio connection retained");
    } catch (...) { disconnect(); throw; }
}

void LinuxBleTransport::disconnect() noexcept {
    _connected = false;
    uint64_t wake = 1;
    const auto written = ::write(_cancelFd, &wake, sizeof(wake));
    (void)written;
    {
        std::scoped_lock lock(_readMutex, _writeMutex, _metadataMutex);
        closeFd(_readFd); closeFd(_writeFd); _pending.clear();
        _client->release();
    }
    // Avoid calling the legacy connector's non-idempotent close on BLE sessions.
    if (!_ble && _classic->isConnected()) _classic->disconnect();
}
bool LinuxBleTransport::isConnected() const noexcept { return _ble ? _connected.load() : _classic->isConnected(); }
ControlBearer LinuxBleTransport::controlBearer() const noexcept { return _ble ? ControlBearer::BleGatt : ControlBearer::Rfcomm; }

DeviceMetadata LinuxBleTransport::deviceMetadata() {
    std::lock_guard lock(_metadataMutex);
    if (!_ble || !_connected) return {};
    return _client->deviceMetadata();
}

void LinuxBleTransport::waitFor(int fd, short events) {
    pollfd fds[] = {{fd, events, 0}, {_cancelFd, POLLIN, 0}};
    int ready;
    do { ready = poll(fds, 2, 2500); } while (ready < 0 && errno == EINTR);
    const std::string operation = events == POLLOUT ? "BLE write" : "BLE notification";
    if (ready == 0) throw SonyException(SonyErrorCode::Timeout, operation + " timed out");
    if (ready < 0 || fds[1].revents || (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))) {
        _connected = false;
        throw SonyException(SonyErrorCode::Disconnected, operation + " link closed");
    }
}

size_t LinuxBleTransport::send(std::span<const std::byte> data) {
    if (!_ble) return _classic->send(data);
    std::lock_guard lock(_writeMutex);
    if (!_connected) throw SonyException(SonyErrorCode::Disconnected, "BLE control disconnected");
    if (data.empty()) return 0;
    waitFor(_writeFd, POLLOUT);
    const auto length = std::min(data.size(), static_cast<size_t>(_writeMtu));
    const auto sent = ::send(_writeFd, data.data(), length, MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            throw SonyException(SonyErrorCode::Timeout, "BLE write temporarily unavailable");
        _connected = false;
        throw SonyException(SonyErrorCode::Disconnected, std::string("BLE write failed: ") + strerror(errno));
    }
    return static_cast<size_t>(sent);
}

size_t LinuxBleTransport::receive(std::span<std::byte> buffer) {
    if (!_ble) return _classic->receive(buffer);
    std::lock_guard lock(_readMutex);
    if (!_connected) throw SonyException(SonyErrorCode::Disconnected, "BLE control disconnected");
    if (buffer.empty()) return 0;
    if (_pending.empty()) {
        waitFor(_readFd, POLLIN);
        std::array<std::byte, 65535> packet;
        const auto count = ::read(_readFd, packet.data(), packet.size());
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
            throw SonyException(SonyErrorCode::Timeout, "BLE notification temporarily unavailable");
        if (count <= 0) { _connected = false; throw SonyException(SonyErrorCode::Disconnected, "BLE notification link closed"); }
        _pending.assign(packet.begin(), packet.begin() + count);
    }
    const auto count = std::min(buffer.size(), _pending.size());
    std::copy_n(_pending.begin(), count, buffer.begin());
    _pending.erase(_pending.begin(), _pending.begin() + count);
    return count;
}
} // namespace sony::transport
