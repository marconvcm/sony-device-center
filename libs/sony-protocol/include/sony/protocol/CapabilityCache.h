#pragma once

#include "DeviceProfile.h"
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sony::protocol {

class CapabilityCache {
public:
    CapabilityCache() = default;
    explicit CapabilityCache(std::filesystem::path storagePath);

    static std::string makeKey(
        SonyModel model,
        std::string_view firmwareVersion,
        std::string_view address = "") noexcept;

    [[nodiscard]] std::optional<DeviceCapabilities> get(std::string_view key) const;
    void put(std::string_view key, const DeviceCapabilities& capabilities);

    [[nodiscard]] bool has(std::string_view key) const;
    void clear();

    bool loadFromFile(const std::filesystem::path& path);
    bool saveToFile(const std::filesystem::path& path) const;

    [[nodiscard]] size_t size() const;

private:
    mutable std::mutex _mutex;
    std::filesystem::path _storagePath;
    std::unordered_map<std::string, DeviceCapabilities> _cache;
};

} // namespace sony::protocol
