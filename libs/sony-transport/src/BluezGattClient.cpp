#include "sony/transport/LinuxBleTransport.h"
#include "BluezGattObjects.h"
#include "sony/transport/SonyError.h"
#include "sony/transport/Logger.h"
#include <dbus/dbus.h>
#include <fcntl.h>
#include <map>
#include <utility>
#include <unistd.h>

namespace sony::transport {
namespace {
using Message = std::unique_ptr<DBusMessage, decltype(&dbus_message_unref)>;
std::string stringValue(DBusMessageIter& iter) {
    const char* value = nullptr;
    dbus_message_iter_get_basic(&iter, &value);
    return value ? value : "";
}
std::string byteArrayValue(DBusMessageIter& iter) {
    DBusMessageIter bytes;
    dbus_message_iter_recurse(&iter, &bytes);
    const char* data = nullptr;
    int size = 0;
    dbus_message_iter_get_fixed_array(&bytes, &data, &size);
    return size > 0 ? std::string(data, size) : std::string{};
}
class BluezGattClient final : public IBluezGattClient {
    DBusConnection* _bus{nullptr};
    std::string _devicePath;
    Message call(const std::string& path, const char* interface, const char* method, bool options = false) {
        Message request(dbus_message_new_method_call("org.bluez", path.c_str(), interface, method), dbus_message_unref);
        if (!request) throw SonyException(SonyErrorCode::TransportFailure, "Cannot allocate BlueZ request");
        if (options) {
            DBusMessageIter args, dictionary;
            dbus_message_iter_init_append(request.get(), &args);
            dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dictionary);
            dbus_message_iter_close_container(&args, &dictionary);
        }
        DBusError error;
        dbus_error_init(&error);
        auto* reply = dbus_connection_send_with_reply_and_block(_bus, request.get(), 3000, &error);
        if (!reply) {
            std::string detail = error.message ? error.message : "no reply";
            dbus_error_free(&error);
            throw SonyException(SonyErrorCode::TransportFailure, std::string("BlueZ ") + method + ": " + detail);
        }
        return Message(reply, dbus_message_unref);
    }
    std::map<std::string, detail::BluezObject> readObjects() {
        auto reply = call("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        if (!dbus_message_has_signature(reply.get(), "a{oa{sa{sv}}}"))
            throw SonyException(SonyErrorCode::InvalidResponse, "Malformed BlueZ object list");
        std::map<std::string, detail::BluezObject> objects;
        DBusMessageIter top, entries;
        dbus_message_iter_init(reply.get(), &top);
        dbus_message_iter_recurse(&top, &entries);
        while (dbus_message_iter_get_arg_type(&entries) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entry, interfaces;
            dbus_message_iter_recurse(&entries, &entry);
            const auto path = stringValue(entry);
            auto& object = objects[path];
            dbus_message_iter_next(&entry);
            dbus_message_iter_recurse(&entry, &interfaces);
            while (dbus_message_iter_get_arg_type(&interfaces) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter iface, properties;
                dbus_message_iter_recurse(&interfaces, &iface);
                const auto prefix = stringValue(iface) + "/";
                dbus_message_iter_next(&iface);
                dbus_message_iter_recurse(&iface, &properties);
                while (dbus_message_iter_get_arg_type(&properties) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter prop, value;
                    dbus_message_iter_recurse(&properties, &prop);
                    const auto key = prefix + stringValue(prop);
                    dbus_message_iter_next(&prop);
                    dbus_message_iter_recurse(&prop, &value);
                    const auto type = dbus_message_iter_get_arg_type(&value);
                    if (type == DBUS_TYPE_STRING || type == DBUS_TYPE_OBJECT_PATH) object.strings[key] = stringValue(value);
                    else if (type == DBUS_TYPE_BOOLEAN) {
                        dbus_bool_t b;
                        dbus_message_iter_get_basic(&value, &b); object.booleans[key] = b;
                    } else if (type == DBUS_TYPE_BYTE) {
                        unsigned char b;
                        dbus_message_iter_get_basic(&value, &b); object.bytes[key] = b;
                    } else if (type == DBUS_TYPE_ARRAY && dbus_message_iter_get_element_type(&value) == DBUS_TYPE_BYTE) {
                        // GATT Value holds the firmware text; retain embedded NULs for validation.
                        if (key == "org.bluez.GattCharacteristic1/Value") object.strings[key] = byteArrayValue(value);
                    } else if (type == DBUS_TYPE_ARRAY && dbus_message_iter_get_element_type(&value) == DBUS_TYPE_STRING) {
                        DBusMessageIter items;
                        dbus_message_iter_recurse(&value, &items);
                        while (dbus_message_iter_get_arg_type(&items) == DBUS_TYPE_STRING) {
                            object.arrays[key].push_back(stringValue(items)); dbus_message_iter_next(&items);
                        }
                    }
                    dbus_message_iter_next(&properties);
                }
                dbus_message_iter_next(&interfaces);
            }
            dbus_message_iter_next(&entries);
        }
        return objects;
    }
public:
    ~BluezGattClient() override { release(); }
    void release() noexcept override {
        _devicePath.clear();
        if (_bus) { dbus_connection_close(_bus); dbus_connection_unref(_bus); _bus = nullptr; }
    }
    std::optional<GattEndpoint> resolve(const DeviceAddress& address) override {
        release();
        dbus_threads_init_default();
        DBusError error;
        dbus_error_init(&error);
        _bus = dbus_bus_get_private(DBUS_BUS_SYSTEM, &error);
        if (!_bus) {
            std::string detail = error.message ? error.message : "unavailable";
            dbus_error_free(&error);
            throw SonyException(SonyErrorCode::TransportFailure, "BlueZ system bus: " + detail);
        }
        dbus_connection_set_exit_on_disconnect(_bus, false);
        auto endpoint = detail::selectGattEndpoint(readObjects(), address);
        if (endpoint) _devicePath = endpoint->devicePath;
        return endpoint;
    }

    DeviceMetadata deviceMetadata() override {
        if (!_bus || _devicePath.empty()) return {};
        auto objects = readObjects();
        auto result = detail::readDeviceMetadata(objects, _devicePath);
        const auto device = objects.find(_devicePath);
        if (device == objects.end() || !device->second.boolean("org.bluez.Device1/Connected") ||
            !device->second.boolean("org.bluez.Device1/ServicesResolved")) return result;
        const auto path = detail::firmwareCharacteristic(objects, _devicePath);
        if (result.firmware.empty() && !path.empty()) {
            // Value is optional until the first read. Never write or acquire an audio transport.
            try {
                auto reply = call(path, "org.bluez.GattCharacteristic1", "ReadValue", true);
                if (dbus_message_has_signature(reply.get(), "ay")) {
                    DBusMessageIter args;
                    dbus_message_iter_init(reply.get(), &args);
                    result.firmware = detail::firmwareString(byteArrayValue(args));
                }
            } catch (const SonyException& ex) {
                Logger::debug(LogCategory::Transport, ex.what());
            }
        }
        return result;
    }

    GattDescriptor acquire(const std::string& path, bool notify) override {
        auto reply = call(path, "org.bluez.GattCharacteristic1", notify ? "AcquireNotify" : "AcquireWrite", true);
        if (!dbus_message_has_signature(reply.get(), "hq"))
            throw SonyException(SonyErrorCode::InvalidResponse, "Malformed BlueZ GATT descriptor reply");
        DBusMessageIter args;
        dbus_message_iter_init(reply.get(), &args);
        int fd; dbus_uint16_t mtu;
        dbus_message_iter_get_basic(&args, &fd);
        dbus_message_iter_next(&args); dbus_message_iter_get_basic(&args, &mtu);
        const int owned = fcntl(fd, F_DUPFD_CLOEXEC, 0);
        if (owned < 0) throw SonyException(SonyErrorCode::TransportFailure, "Cannot retain BlueZ GATT descriptor");
        return {owned, mtu};
    }
};
}
std::unique_ptr<IBluezGattClient> createBluezGattClient() { return std::make_unique<BluezGattClient>(); }
} // namespace sony::transport
