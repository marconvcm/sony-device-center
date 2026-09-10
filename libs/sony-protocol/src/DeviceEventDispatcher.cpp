#include "sony/protocol/DeviceEventDispatcher.h"
#include <algorithm>

namespace sony::protocol {

DeviceEventDispatcher::SubscriptionId DeviceEventDispatcher::onBatteryChanged(BatteryHandler handler) {
    std::lock_guard lock(_mutex);
    auto id = _nextId++;
    _batteryHandlers[id] = std::move(handler);
    return id;
}

DeviceEventDispatcher::SubscriptionId DeviceEventDispatcher::onNoiseControlChanged(NoiseControlHandler handler) {
    std::lock_guard lock(_mutex);
    auto id = _nextId++;
    _noiseControlHandlers[id] = std::move(handler);
    return id;
}

DeviceEventDispatcher::SubscriptionId DeviceEventDispatcher::onEqualizerChanged(EqualizerHandler handler) {
    std::lock_guard lock(_mutex);
    auto id = _nextId++;
    _equalizerHandlers[id] = std::move(handler);
    return id;
}

DeviceEventDispatcher::SubscriptionId DeviceEventDispatcher::onConnectionChanged(ConnectionHandler handler) {
    std::lock_guard lock(_mutex);
    auto id = _nextId++;
    _connectionHandlers[id] = std::move(handler);
    return id;
}

DeviceEventDispatcher::SubscriptionId DeviceEventDispatcher::onStateChanged(DeviceStateHandler handler) {
    std::lock_guard lock(_mutex);
    auto id = _nextId++;
    _stateHandlers[id] = std::move(handler);
    return id;
}

void DeviceEventDispatcher::removeListener(SubscriptionId id) {
    std::lock_guard lock(_mutex);
    _batteryHandlers.erase(id);
    _noiseControlHandlers.erase(id);
    _equalizerHandlers.erase(id);
    _connectionHandlers.erase(id);
    _stateHandlers.erase(id);
}

void DeviceEventDispatcher::clear() {
    std::lock_guard lock(_mutex);
    _batteryHandlers.clear();
    _noiseControlHandlers.clear();
    _equalizerHandlers.clear();
    _connectionHandlers.clear();
    _stateHandlers.clear();
}

void DeviceEventDispatcher::dispatch(const BatteryChanged& evt) {
    std::vector<BatteryHandler> handlers;
    {
        std::lock_guard lock(_mutex);
        handlers.reserve(_batteryHandlers.size());
        for (const auto& [id, h] : _batteryHandlers) {
            handlers.push_back(h);
        }
    }
    for (const auto& h : handlers) {
        if (h) h(evt);
    }
}

void DeviceEventDispatcher::dispatch(const NoiseControlChanged& evt) {
    std::vector<NoiseControlHandler> handlers;
    {
        std::lock_guard lock(_mutex);
        handlers.reserve(_noiseControlHandlers.size());
        for (const auto& [id, h] : _noiseControlHandlers) {
            handlers.push_back(h);
        }
    }
    for (const auto& h : handlers) {
        if (h) h(evt);
    }
}

void DeviceEventDispatcher::dispatch(const EqualizerChanged& evt) {
    std::vector<EqualizerHandler> handlers;
    {
        std::lock_guard lock(_mutex);
        handlers.reserve(_equalizerHandlers.size());
        for (const auto& [id, h] : _equalizerHandlers) {
            handlers.push_back(h);
        }
    }
    for (const auto& h : handlers) {
        if (h) h(evt);
    }
}

void DeviceEventDispatcher::dispatch(const ConnectionChanged& evt) {
    std::vector<ConnectionHandler> handlers;
    {
        std::lock_guard lock(_mutex);
        handlers.reserve(_connectionHandlers.size());
        for (const auto& [id, h] : _connectionHandlers) {
            handlers.push_back(h);
        }
    }
    for (const auto& h : handlers) {
        if (h) h(evt);
    }
}

void DeviceEventDispatcher::dispatch(const DeviceStateChanged& evt) {
    std::vector<DeviceStateHandler> handlers;
    {
        std::lock_guard lock(_mutex);
        handlers.reserve(_stateHandlers.size());
        for (const auto& [id, h] : _stateHandlers) {
            handlers.push_back(h);
        }
    }
    for (const auto& h : handlers) {
        if (h) h(evt);
    }
}

bool DeviceEventDispatcher::parseNotification(const SonyFrame& frame, DeviceState& inOutState) {
    return parseNotificationPayload(frame.payload, inOutState);
}

bool DeviceEventDispatcher::parseNotificationPayload(const std::vector<uint8_t>& payload, DeviceState& inOutState) {
    if (payload.empty()) {
        return false;
    }

    uint8_t opcode = payload[0];

    // Battery notification: 0x25 or 0x23
    if (opcode == 0x25 || opcode == 0x23) {
        if (payload.size() >= 4 && payload[1] == 0x00) {
            inOutState.battery.main = static_cast<int>(payload[2]);
            inOutState.battery.charging = (payload[3] == 1);
            dispatch(BatteryChanged{inOutState.battery});
            dispatch(DeviceStateChanged{std::make_shared<const DeviceState>(inOutState)});
            return true;
        }
        if (payload.size() >= 6 && payload[1] == 0x09) {
            inOutState.battery.left = static_cast<int>(payload[2]);
            inOutState.battery.right = static_cast<int>(payload[4]);
            inOutState.battery.main = std::min(static_cast<int>(payload[2]), static_cast<int>(payload[4]));
            inOutState.battery.charging = (payload[3] == 1 || payload[5] == 1);
            dispatch(BatteryChanged{inOutState.battery});
            dispatch(DeviceStateChanged{std::make_shared<const DeviceState>(inOutState)});
            return true;
        }
        if (payload.size() >= 4 && payload[1] == 0x0a) {
            inOutState.battery.caseBattery = static_cast<int>(payload[2]);
            dispatch(BatteryChanged{inOutState.battery});
            dispatch(DeviceStateChanged{std::make_shared<const DeviceState>(inOutState)});
            return true;
        }
    }

    // NC / ASM notification: 0x67 or 0x69
    if (opcode == 0x67 || opcode == 0x69) {
        if (payload.size() >= 7 && payload[1] == 0x17 && payload[2] == 0x01) {
            bool on = (payload[3] != 0);
            bool ambient = (payload[4] != 0);
            bool voice = (payload[5] != 0);
            int level = static_cast<int>(payload[6]);

            inOutState.noiseControl.mode = on ? (ambient ? NoiseControlMode::Ambient : NoiseControlMode::NoiseCancelling) : NoiseControlMode::Off;
            inOutState.noiseControl.ambientLevel = ambient ? level : 0;
            inOutState.noiseControl.focusOnVoice = voice;

            dispatch(NoiseControlChanged{inOutState.noiseControl});
            dispatch(DeviceStateChanged{std::make_shared<const DeviceState>(inOutState)});
            return true;
        }
    }

    // Equalizer notification: 0x57 or 0x59
    if (opcode == 0x57 || opcode == 0x59) {
        if (payload.size() >= 3) {
            inOutState.equalizer.preset = static_cast<int>(payload[2]);
            if (payload.size() >= 10) {
                inOutState.equalizer.clearBass = static_cast<int>(payload[4]) - 10;
                for (size_t i = 0; i < 5; ++i) {
                    inOutState.equalizer.bands[i] = static_cast<int>(payload[5 + i]) - 10;
                }
            }
            dispatch(EqualizerChanged{inOutState.equalizer});
            dispatch(DeviceStateChanged{std::make_shared<const DeviceState>(inOutState)});
            return true;
        }
    }

    return false;
}

} // namespace sony::protocol
