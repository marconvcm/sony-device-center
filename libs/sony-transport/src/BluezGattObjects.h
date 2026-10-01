#pragma once

#include "sony/transport/LinuxBleTransport.h"
#include "sony/transport/SonyError.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>

namespace sony::transport::detail {

inline constexpr auto SonyControlService = "5b833e20-6bc7-4802-8e9a-723ceca4bd8f";
inline constexpr auto SonyControlWrite = "5b833c60-6bc7-4802-8e9a-723ceca4bd8f";
inline constexpr auto SonyControlNotify = "5b833c61-6bc7-4802-8e9a-723ceca4bd8f";

struct BluezObject {
    std::map<std::string, std::string> strings;
    std::map<std::string, bool> booleans;
    std::map<std::string, std::vector<std::string>> arrays;
    std::map<std::string, uint8_t> bytes;

    std::string string(const char* key) const {
        const auto value = strings.find(key);
        return value == strings.end() ? std::string{} : value->second;
    }
    bool boolean(const char* key) const {
        const auto value = booleans.find(key);
        return value != booleans.end() && value->second;
    }
    bool hasFlag(const char* flag) const {
        const auto value = arrays.find("org.bluez.GattCharacteristic1/Flags");
        return value != arrays.end() &&
            std::find(value->second.begin(), value->second.end(), flag) != value->second.end();
    }
};

// Cached GATT services alone never divert a Classic or unknown-model session.
inline std::optional<GattEndpoint> selectGattEndpoint(
    const std::map<std::string, BluezObject>& objects, const DeviceAddress& address) {
    std::string devicePath;
    for (const auto& [path, object] : objects) {
        const auto candidate = object.string("org.bluez.Device1/Address");
        if (!std::equal(candidate.begin(), candidate.end(), address.str().begin(), address.str().end(),
                        [](unsigned char a, unsigned char b) { return std::toupper(a) == std::toupper(b); }))
            continue;
        if (object.string("org.bluez.Device1/Name") != "WH-1000XM6" ||
            !object.boolean("org.bluez.Device1/Paired")) return std::nullopt;

        const bool preferLe = object.string("org.bluez.Device1/PreferredBearer") == "le";
        const bool hasBearer = object.booleans.contains("org.bluez.Bearer.LE1/Connected");
        const bool leConnected = object.boolean("org.bluez.Bearer.LE1/Connected");
        if (!(hasBearer ? leConnected : preferLe)) {
            if (preferLe)
                throw SonyException(SonyErrorCode::Disconnected, "XM6 LE audio is not connected");
            return std::nullopt;
        }
        if (!object.boolean("org.bluez.Device1/Connected"))
            throw SonyException(SonyErrorCode::Disconnected, "XM6 LE audio is not connected");
        if (!object.boolean("org.bluez.Device1/ServicesResolved"))
            throw SonyException(SonyErrorCode::TransportFailure, "Waiting for XM6 GATT service discovery");
        devicePath = path;
        break;
    }
    if (devicePath.empty()) return std::nullopt;

    std::string servicePath;
    for (const auto& [path, object] : objects) {
        if (object.string("org.bluez.GattService1/Device") == devicePath &&
            object.string("org.bluez.GattService1/UUID") == SonyControlService)
            servicePath = path;
    }
    GattEndpoint endpoint{{}, {}, devicePath};
    for (const auto& [path, object] : objects) {
        if (servicePath.empty() || object.string("org.bluez.GattCharacteristic1/Service") != servicePath)
            continue;
        const auto uuid = object.string("org.bluez.GattCharacteristic1/UUID");
        if (uuid == SonyControlWrite && object.hasFlag("write-without-response")) endpoint.writePath = path;
        if (uuid == SonyControlNotify && object.hasFlag("notify")) endpoint.notifyPath = path;
    }
    if (endpoint.writePath.empty() || endpoint.notifyPath.empty())
        throw SonyException(SonyErrorCode::Unsupported, "XM6 LE audio connected, but Sony BLE control service is unavailable");
    return endpoint;
}

inline std::string firmwareCharacteristic(const std::map<std::string, BluezObject>& objects,
                                          const std::string& devicePath) {
    for (const auto& [path, object] : objects) {
        if (object.string("org.bluez.GattCharacteristic1/UUID") != "00002a26-0000-1000-8000-00805f9b34fb" ||
            !object.hasFlag("read")) continue;
        const auto service = objects.find(object.string("org.bluez.GattCharacteristic1/Service"));
        if (service != objects.end() && service->second.string("org.bluez.GattService1/Device") == devicePath &&
            service->second.string("org.bluez.GattService1/UUID") == "0000180a-0000-1000-8000-00805f9b34fb")
            return path;
    }
    return {};
}

inline std::string firmwareString(std::string value) {
    if (value.empty() || value.size() > 64 ||
        !std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c <= 126; })) return {};
    return value;
}

inline DeviceMetadata readDeviceMetadata(const std::map<std::string, BluezObject>& objects,
                                         const std::string& devicePath) {
    DeviceMetadata result;
    const auto device = objects.find(devicePath);
    if (devicePath.empty() || device == objects.end() ||
        !device->second.boolean("org.bluez.Device1/Connected") ||
        !device->second.boolean("org.bluez.Device1/ServicesResolved")) return result;
    const auto battery = device->second.bytes.find("org.bluez.Battery1/Percentage");
    if (battery != device->second.bytes.end() && battery->second <= 100)
        result.batteryPercentage = battery->second;
    for (const auto& [path, object] : objects) {
        if (object.string("org.bluez.MediaTransport1/Device") != devicePath ||
            object.string("org.bluez.MediaTransport1/State") != "active") continue;
        const auto uuid = object.string("org.bluez.MediaTransport1/UUID");
        const auto codec = object.bytes.find("org.bluez.MediaTransport1/Codec");
        // Codec numbers are profile-specific: 0x06 means LC3 only for BAP.
        if ((uuid == "00002bcb-0000-1000-8000-00805f9b34fb" ||
             uuid == "00002bc9-0000-1000-8000-00805f9b34fb") &&
            codec != object.bytes.end() && codec->second == 0x06) result.codec = "LC3";
    }
    const auto firmware = objects.find(firmwareCharacteristic(objects, devicePath));
    if (firmware != objects.end())
        result.firmware = firmwareString(firmware->second.string("org.bluez.GattCharacteristic1/Value"));
    return result;
}

} // namespace sony::transport::detail
