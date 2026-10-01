#pragma once

#include "sony/transport/LinuxBleTransport.h"
#include "sony/transport/SonyError.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace sony::transport::detail {

inline constexpr auto SonyControlService = "5b833e20-6bc7-4802-8e9a-723ceca4bd8f";
inline constexpr auto SonyControlWrite = "5b833c60-6bc7-4802-8e9a-723ceca4bd8f";
inline constexpr auto SonyControlNotify = "5b833c61-6bc7-4802-8e9a-723ceca4bd8f";

struct BluezObject {
    std::map<std::string, std::string> strings;
    std::map<std::string, bool> booleans;
    std::map<std::string, std::vector<std::string>> arrays;

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
    GattEndpoint endpoint;
    for (const auto& [path, object] : objects) {
        if (servicePath.empty() || object.string("org.bluez.GattCharacteristic1/Service") != servicePath)
            continue;
        const auto uuid = object.string("org.bluez.GattCharacteristic1/UUID");
        if (uuid == SonyControlWrite && object.hasFlag("write-without-response")) endpoint.writePath = path;
        if (uuid == SonyControlNotify && object.hasFlag("notify")) endpoint.notifyPath = path;
    }
    if (endpoint.writePath.empty() || endpoint.notifyPath.empty()) {
        if (!objects.at(devicePath).boolean("org.bluez.Device1/ServicesResolved"))
            throw SonyException(SonyErrorCode::TransportFailure, "Waiting for XM6 GATT service discovery");
        throw SonyException(SonyErrorCode::Unsupported, "XM6 LE audio connected, but Sony BLE control service is unavailable");
    }
    // BlueZ can leave the aggregate ServicesResolved flag false on a live LE
    // connection. The complete device-scoped endpoints are enough to attempt
    // acquisition; AcquireNotify/AcquireWrite still verify that GATT is usable.
    return endpoint;
}

} // namespace sony::transport::detail
