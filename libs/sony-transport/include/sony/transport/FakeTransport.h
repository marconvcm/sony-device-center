#pragma once

#include "ITransport.h"
#include "IDeviceDiscovery.h"
#include "SonyError.h"

#include <cstdint>
#include <deque>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace sony::transport {

class FakeTransport : public ITransport {
public:
    FakeTransport();
    ~FakeTransport() override = default;

    // --- ITransport overrides ---
    void connect(const DeviceAddress& address) override;
    void disconnect() noexcept override;
    [[nodiscard]] bool isConnected() const noexcept override;
    size_t send(std::span<const std::byte> data) override;
    size_t receive(std::span<std::byte> buffer) override;

    // --- Testing API: Incoming Data Simulation ---
    void queueIncoming(std::span<const uint8_t> bytes);
    void queueIncoming(std::span<const std::byte> bytes);
    void queueIncoming(const std::vector<uint8_t>& bytes);
    void queueIncoming(const std::vector<std::byte>& bytes);
    void queueIncoming(std::initializer_list<uint8_t> bytes);
    void queueIncoming(const std::vector<std::vector<uint8_t>>& frames);
    void clearIncoming();
    [[nodiscard]] size_t incomingBytesAvailable() const noexcept;

    // --- Testing API: Outgoing Data Inspection ---
    [[nodiscard]] std::vector<std::vector<uint8_t>> sentFrames() const;
    [[nodiscard]] std::vector<std::vector<std::byte>> sentFramesBytes() const;
    [[nodiscard]] size_t sentCount() const noexcept;
    [[nodiscard]] std::vector<uint8_t> lastSentFrame() const;
    [[nodiscard]] std::vector<uint8_t> allSentBytes() const;
    void clearSent();

    // --- Testing API: Fault and Behavior Simulation ---
    void setConnected(bool connected) noexcept;
    void simulateDisconnect() noexcept;
    void setFailConnect(bool fail, SonyErrorCode code = SonyErrorCode::TransportFailure);
    void simulateTimeoutOnReceive(bool enable = true, size_t count = 1);
    void simulateTimeoutOnSend(bool enable = true, size_t count = 1);
    void setMaxReceiveChunkSize(std::optional<size_t> maxChunkSize) noexcept;

    [[nodiscard]] const DeviceAddress& connectedAddress() const noexcept;

private:
    mutable std::mutex _mutex;
    bool _connected{false};
    DeviceAddress _connectedAddress;

    std::deque<uint8_t> _incomingQueue;
    std::vector<std::vector<uint8_t>> _sentFrames;

    bool _failConnect{false};
    SonyErrorCode _failConnectCode{SonyErrorCode::TransportFailure};

    bool _simulateTimeoutOnReceive{false};
    size_t _timeoutReceiveCount{0};

    bool _simulateTimeoutOnSend{false};
    size_t _timeoutSendCount{0};

    std::optional<size_t> _maxReceiveChunkSize;
};

class FakeDeviceDiscovery : public IDeviceDiscovery {
public:
    FakeDeviceDiscovery() = default;
    ~FakeDeviceDiscovery() override = default;

    void addDevice(DiscoveredDevice device);
    void setDevices(std::vector<DiscoveredDevice> devices);
    void clear();

    std::vector<DiscoveredDevice> discover() override;

private:
    std::mutex _mutex;
    std::vector<DiscoveredDevice> _devices;
};

} // namespace sony::transport
