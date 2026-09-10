#include "sony/protocol/CapabilityDiscovery.h"

namespace sony::protocol {

CapabilityDiscovery::CapabilityDiscovery(std::shared_ptr<CapabilityCache> cache)
    : _cache(std::move(cache)) {
    if (!_cache) {
        _cache = std::make_shared<CapabilityCache>();
    }
}

DeviceCapabilities CapabilityDiscovery::discover(
    IProtocol& protocol,
    std::string_view deviceName,
    std::string_view address)
{
    SonyModel model = DeviceProfileRegistry::identifyModel(deviceName);

    // 1. Fast path: If device is known in profile registry, return capabilities immediately.
    // Zero latency, zero packets sent, zero timeout wait!
    if (DeviceProfileRegistry::isKnownDevice(model)) {
        auto profile = DeviceProfileRegistry::getProfile(model);
        if (profile.has_value()) {
            return profile->capabilities;
        }
    }

    // 2. Check persistent capability cache
    std::string cacheKey = CapabilityCache::makeKey(model, "", address);
    auto cached = _cache->get(cacheKey);
    if (cached.has_value()) {
        return *cached;
    }

    // 3. Fallback: Probe unknown device
    return probeDevice(protocol, model, address);
}

std::future<DeviceCapabilities> CapabilityDiscovery::discoverAsync(
    IProtocol& protocol,
    std::string_view deviceName,
    std::string_view address)
{
    return std::async(std::launch::async, [this, &protocol, name = std::string(deviceName), addr = std::string(address)]() {
        return discover(protocol, name, addr);
    });
}

DeviceCapabilities CapabilityDiscovery::probeDevice(
    IProtocol& protocol,
    SonyModel model,
    std::string_view address)
{
    DeviceCapabilities caps;
    std::string firmwareVersion;

    try {
        firmwareVersion = protocol.getFirmwareVersion();
        if (!firmwareVersion.empty()) {
            caps.firmwareInfo = true;
        }
    } catch (...) {}

    try {
        auto codec = protocol.getCodec();
        if (!codec.empty()) {
            caps.codecInfo = true;
        }
    } catch (...) {}

    try {
        auto battery = protocol.getBattery();
        if (battery.main.has_value() || battery.left.has_value()) {
            caps.battery = true;
            if (battery.left.has_value() && battery.right.has_value()) {
                caps.dualBattery = true;
            }
        }
    } catch (...) {}

    try {
        protocol.getNoiseControl();
        caps.noiseCancelling = true;
        caps.ambientSound = true;
        caps.focusOnVoice = true;
    } catch (...) {}

    try {
        protocol.getEqualizer();
        caps.equalizer = true;
        caps.clearBass = true;
    } catch (...) {}

    try {
        protocol.getDsee();
        caps.dsee = true;
    } catch (...) {}

    try {
        protocol.getSpeakToChat();
        caps.speakToChat = true;
    } catch (...) {}

    try {
        protocol.getAdaptiveVolume();
        caps.adaptiveVolume = true;
    } catch (...) {}

    try {
        protocol.getAutoPowerOff();
        caps.autoPowerOff = true;
    } catch (...) {}

    // Save probed capabilities to cache
    std::string cacheKey = CapabilityCache::makeKey(model, firmwareVersion, address);
    _cache->put(cacheKey, caps);

    return caps;
}

} // namespace sony::protocol
