#pragma once

#include "IDeviceService.h"
#include "sony/transport/IDeviceDiscovery.h"
#include <mutex>
#include <chrono>
#include <functional>

namespace sony::core {

class DeviceService : public IDeviceService {
public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;
    explicit DeviceService(
        std::shared_ptr<transport::ITransport> transport,
        std::shared_ptr<transport::IDeviceDiscovery> discovery = nullptr,
        Now now = [] { return Clock::now(); });
    ~DeviceService() override;

    void tick() override;
    void startAutoConnect(std::string address = {}) override;
    std::string connectionState() const override;
    std::string selectedAddress() const override;
    std::string lastError() const override;
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
    mutable std::recursive_mutex _mutex;
    Now _now;
    bool _automatic{false};
    bool _wasConnected{false};
    unsigned _retrySeconds{1};
    Clock::time_point _nextAttempt{}, _nextSettings{}, _nextBattery{};
    std::string _target, _selected, _connectionState{"disconnected"}, _lastError;
    void _connect(const transport::DeviceAddress& address, std::string_view name);

};

} // namespace sony::core
