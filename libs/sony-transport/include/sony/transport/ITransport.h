#pragma once

#include "DeviceAddress.h"
#include <cstddef>
#include <span>
#include <optional>
#include <string>

namespace sony::transport {

enum class ControlBearer { Unknown, Rfcomm, BleGatt };

// Optional host metadata, independent of Sony MDR protocol commands.
struct DeviceMetadata {
    std::optional<int> batteryPercentage;
    std::string codec;
    std::string firmware;
};

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual void connect(const DeviceAddress& address) = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
    [[nodiscard]] virtual ControlBearer controlBearer() const noexcept { return ControlBearer::Unknown; }
    virtual DeviceMetadata deviceMetadata() { return {}; }
    virtual size_t send(std::span<const std::byte> data) = 0;
    virtual size_t receive(std::span<std::byte> buffer) = 0;
};

} // namespace sony::transport
