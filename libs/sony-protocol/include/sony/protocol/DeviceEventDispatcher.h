#pragma once

#include "DeviceEvents.h"
#include "SonyFrame.h"
#include "DeviceState.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>
#include <unordered_map>

namespace sony::protocol {

class DeviceEventDispatcher {
public:
    using SubscriptionId = uint64_t;

    using BatteryHandler = std::function<void(const BatteryChanged&)>;
    using NoiseControlHandler = std::function<void(const NoiseControlChanged&)>;
    using EqualizerHandler = std::function<void(const EqualizerChanged&)>;
    using ConnectionHandler = std::function<void(const ConnectionChanged&)>;
    using DeviceStateHandler = std::function<void(const DeviceStateChanged&)>;

    DeviceEventDispatcher() = default;
    ~DeviceEventDispatcher() = default;

    DeviceEventDispatcher(const DeviceEventDispatcher&) = delete;
    DeviceEventDispatcher& operator=(const DeviceEventDispatcher&) = delete;

    SubscriptionId onBatteryChanged(BatteryHandler handler);
    SubscriptionId onNoiseControlChanged(NoiseControlHandler handler);
    SubscriptionId onEqualizerChanged(EqualizerHandler handler);
    SubscriptionId onConnectionChanged(ConnectionHandler handler);
    SubscriptionId onStateChanged(DeviceStateHandler handler);

    void removeListener(SubscriptionId id);
    void clear();

    void dispatch(const BatteryChanged& evt);
    void dispatch(const NoiseControlChanged& evt);
    void dispatch(const EqualizerChanged& evt);
    void dispatch(const ConnectionChanged& evt);
    void dispatch(const DeviceStateChanged& evt);

    // Parses an incoming unsolicited frame/payload, mutates the state, and dispatches events.
    // Returns true if the notification was recognized and handled.
    bool parseNotification(const SonyFrame& frame, DeviceState& inOutState);
    bool parseNotificationPayload(const std::vector<uint8_t>& payload, DeviceState& inOutState);

private:
    std::mutex _mutex;
    SubscriptionId _nextId{1};

    std::unordered_map<SubscriptionId, BatteryHandler> _batteryHandlers;
    std::unordered_map<SubscriptionId, NoiseControlHandler> _noiseControlHandlers;
    std::unordered_map<SubscriptionId, EqualizerHandler> _equalizerHandlers;
    std::unordered_map<SubscriptionId, ConnectionHandler> _connectionHandlers;
    std::unordered_map<SubscriptionId, DeviceStateHandler> _stateHandlers;
};

} // namespace sony::protocol
