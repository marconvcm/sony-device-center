#pragma once

#include "DeviceProfile.h"
#include <optional>
#include <string_view>
#include <vector>

namespace sony::protocol {

class DeviceProfileRegistry {
public:
    // Identify SonyModel from Bluetooth advertised name (e.g. "WH-1000XM4", "LE_WH-1000XM5")
    [[nodiscard]] static SonyModel identifyModel(std::string_view deviceName) noexcept;

    // Lookup known profile by SonyModel
    [[nodiscard]] static std::optional<DeviceProfile> getProfile(SonyModel model) noexcept;

    // Lookup known profile directly by device name (returns fallback profile with Unknown model if not found)
    [[nodiscard]] static DeviceProfile getProfileForDevice(std::string_view deviceName) noexcept;

    // Check if device model has known immediate capabilities (no probing needed)
    [[nodiscard]] static bool isKnownDevice(SonyModel model) noexcept;

    // List all registered device profiles
    [[nodiscard]] static const std::vector<DeviceProfile>& allProfiles() noexcept;
};

} // namespace sony::protocol
