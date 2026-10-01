#pragma once

#include "ITransport.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace sony::transport {

struct GattEndpoint { std::string writePath; std::string notifyPath; std::string devicePath; };
struct GattDescriptor { int fd; unsigned mtu; };

// Acquired descriptors belong to the caller; the client keeps its bus connection
// alive until release(). None of these operations disconnect the shared device.
class IBluezGattClient {
public:
    virtual ~IBluezGattClient() = default;
    virtual std::optional<GattEndpoint> resolve(const DeviceAddress&) = 0;
    virtual GattDescriptor acquire(const std::string& path, bool notify) = 0;
    virtual DeviceMetadata deviceMetadata() = 0;
    virtual void release() noexcept = 0;
};

std::unique_ptr<IBluezGattClient> createBluezGattClient();

class LinuxBleTransport final : public ITransport {
public:
    LinuxBleTransport(std::unique_ptr<ITransport> classic, std::unique_ptr<IBluezGattClient> client);
    ~LinuxBleTransport() override;
    void connect(const DeviceAddress& address) override;
    void disconnect() noexcept override;
    bool isConnected() const noexcept override;
    ControlBearer controlBearer() const noexcept override;
    DeviceMetadata deviceMetadata() override;
    size_t send(std::span<const std::byte> data) override;
    size_t receive(std::span<std::byte> buffer) override;
private:
    void waitFor(int fd, short events);
    std::unique_ptr<ITransport> _classic;
    std::unique_ptr<IBluezGattClient> _client;
    std::atomic<bool> _ble{false}, _connected{false};
    int _readFd{-1}, _writeFd{-1}, _cancelFd{-1};
    unsigned _writeMtu{0};
    std::mutex _readMutex, _writeMutex, _metadataMutex;
    std::vector<std::byte> _pending;
};

} // namespace sony::transport
