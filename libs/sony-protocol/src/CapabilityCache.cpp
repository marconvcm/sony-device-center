#include "sony/protocol/CapabilityCache.h"
#include <fstream>
#include <sstream>
#include <vector>

namespace sony::protocol {

namespace {

std::string serializeCapabilities(const DeviceCapabilities& caps) {
    std::ostringstream ss;
    ss << (caps.battery ? "1" : "0") << ","
       << (caps.dualBattery ? "1" : "0") << ","
       << (caps.noiseCancelling ? "1" : "0") << ","
       << (caps.ambientSound ? "1" : "0") << ","
       << (caps.focusOnVoice ? "1" : "0") << ","
       << (caps.equalizer ? "1" : "0") << ","
       << (caps.clearBass ? "1" : "0") << ","
       << (caps.dsee ? "1" : "0") << ","
       << (caps.speakToChat ? "1" : "0") << ","
       << (caps.adaptiveVolume ? "1" : "0") << ","
       << (caps.autoPowerOff ? "1" : "0") << ","
       << (caps.firmwareInfo ? "1" : "0") << ","
       << (caps.codecInfo ? "1" : "0") << ","
       << (caps.wearSensor ? "1" : "0") << ","
       << (caps.multipoint ? "1" : "0");
    return ss.str();
}

std::optional<DeviceCapabilities> deserializeCapabilities(std::string_view str) {
    std::string s(str);
    std::stringstream ss(s);
    std::string token;
    std::vector<bool> bits;
    while (std::getline(ss, token, ',')) {
        bits.push_back(token == "1");
    }
    if (bits.size() < 15) {
        return std::nullopt;
    }
    DeviceCapabilities caps;
    caps.battery = bits[0];
    caps.dualBattery = bits[1];
    caps.noiseCancelling = bits[2];
    caps.ambientSound = bits[3];
    caps.focusOnVoice = bits[4];
    caps.equalizer = bits[5];
    caps.clearBass = bits[6];
    caps.dsee = bits[7];
    caps.speakToChat = bits[8];
    caps.adaptiveVolume = bits[9];
    caps.autoPowerOff = bits[10];
    caps.firmwareInfo = bits[11];
    caps.codecInfo = bits[12];
    caps.wearSensor = bits[13];
    caps.multipoint = bits[14];
    return caps;
}

} // namespace

CapabilityCache::CapabilityCache(std::filesystem::path storagePath)
    : _storagePath(std::move(storagePath)) {
    if (!_storagePath.empty() && std::filesystem::exists(_storagePath)) {
        loadFromFile(_storagePath);
    }
}

std::string CapabilityCache::makeKey(
    SonyModel model,
    std::string_view firmwareVersion,
    std::string_view address) noexcept
{
    std::string key = std::string(to_string(model));
    if (!firmwareVersion.empty()) {
        key += "@" + std::string(firmwareVersion);
    }
    if (!address.empty()) {
        key += "#" + std::string(address);
    }
    return key;
}

std::optional<DeviceCapabilities> CapabilityCache::get(std::string_view key) const {
    std::lock_guard lock(_mutex);
    auto it = _cache.find(std::string(key));
    if (it != _cache.end()) {
        return it->second;
    }
    return std::nullopt;
}

void CapabilityCache::put(std::string_view key, const DeviceCapabilities& capabilities) {
    std::lock_guard lock(_mutex);
    _cache[std::string(key)] = capabilities;
    if (!_storagePath.empty()) {
        saveToFile(_storagePath);
    }
}

bool CapabilityCache::has(std::string_view key) const {
    std::lock_guard lock(_mutex);
    return _cache.find(std::string(key)) != _cache.end();
}

void CapabilityCache::clear() {
    std::lock_guard lock(_mutex);
    _cache.clear();
    if (!_storagePath.empty()) {
        saveToFile(_storagePath);
    }
}

size_t CapabilityCache::size() const {
    std::lock_guard lock(_mutex);
    return _cache.size();
}

bool CapabilityCache::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::unordered_map<std::string, DeviceCapabilities> loaded;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto delimiter = line.find('=');
        if (delimiter != std::string::npos) {
            std::string key = line.substr(0, delimiter);
            std::string val = line.substr(delimiter + 1);
            auto caps = deserializeCapabilities(val);
            if (caps.has_value()) {
                loaded[key] = *caps;
            }
        }
    }

    std::lock_guard lock(_mutex);
    _cache = std::move(loaded);
    return true;
}

bool CapabilityCache::saveToFile(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::lock_guard lock(_mutex);
    for (const auto& [key, caps] : _cache) {
        file << key << "=" << serializeCapabilities(caps) << "\n";
    }
    return true;
}

} // namespace sony::protocol
