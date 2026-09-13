#pragma once

#include "sony/transport/IDeviceDiscovery.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace sony::transport {

/// Gives the first three octets (the OUI) of a Bluetooth address as a 24-bit value.
/// Accepts ':' or '-' separators and hex digits in upper or lower case, so that
/// "AC:80:0A:12:34:56" (Linux), "ac:80:0a:12:34:56" (Windows), and
/// "ac-80-0a-12-34-56" (macOS) give the same value. Gives std::nullopt for other text.
std::optional<std::uint32_t> addressOui(std::string_view address);

/// True when the IEEE registry assigns the address prefix to a Sony audio or
/// electronics company. Sony Interactive Entertainment (PlayStation) is excluded.
bool hasSonyOui(std::string_view address);

/// True when the name contains "Sony" or a Sony model prefix such as "WH-".
bool hasSonyName(std::string_view name);

/// True when the address prefix or the name identifies a Sony device. The name
/// check finds a Sony model with an address outside the OUI table, and the address
/// check finds a device that the user renamed.
bool isSonyCandidate(const DiscoveredDevice& device);

/// Wraps a platform discovery and keeps only the devices that isSonyCandidate()
/// accepts. The service tries each discovered device, so an unfiltered list makes it
/// open connections to phones, speakers, and other headphones.
class SonyDeviceDiscovery : public IDeviceDiscovery {
public:
    explicit SonyDeviceDiscovery(std::unique_ptr<IDeviceDiscovery> inner);

    std::vector<DiscoveredDevice> discover() override;

private:
    std::unique_ptr<IDeviceDiscovery> _inner;
};

} // namespace sony::transport
