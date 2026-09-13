#include "sony/transport/SonyDeviceFilter.h"
#include "SonyOuiTable.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace sony::transport {

namespace {

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

constexpr std::array<std::string_view, 7> kSonyNameTokens = {
    "WH-", "WF-", "WI-", "MDR-", "LINKBUDS", "ULT WEAR", "SONY",
};

} // namespace

std::optional<std::uint32_t> addressOui(std::string_view address) {
    // Six octets of two hex digits, with a separator after each of the first five.
    constexpr std::size_t kAddressLength = 17;
    constexpr std::size_t kOuiLength = 8;
    if (address.size() != kAddressLength) return std::nullopt;

    std::uint32_t oui = 0;
    for (std::size_t i = 0; i < address.size(); ++i) {
        const char c = address[i];
        if (i % 3 == 2) {
            if (c != ':' && c != '-') return std::nullopt;
            continue;
        }
        const int digit = hexValue(c);
        if (digit < 0) return std::nullopt;
        if (i < kOuiLength) oui = (oui << 4) | static_cast<std::uint32_t>(digit);
    }
    return oui;
}

bool hasSonyOui(std::string_view address) {
    const auto oui = addressOui(address);
    return oui && std::binary_search(detail::kSonyOuis.begin(), detail::kSonyOuis.end(), *oui);
}

bool hasSonyName(std::string_view name) {
    std::string upper(name);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return std::any_of(kSonyNameTokens.begin(), kSonyNameTokens.end(), [&](std::string_view token) {
        return upper.find(token) != std::string::npos;
    });
}

bool isSonyCandidate(const DiscoveredDevice& device) {
    return hasSonyOui(device.address.str()) || hasSonyName(device.name);
}

SonyDeviceDiscovery::SonyDeviceDiscovery(std::unique_ptr<IDeviceDiscovery> inner)
    : _inner(std::move(inner)) {}

std::vector<DiscoveredDevice> SonyDeviceDiscovery::discover() {
    if (!_inner) {
        return {};
    }
    auto devices = _inner->discover();
    std::erase_if(devices, [](const DiscoveredDevice& device) { return !isSonyCandidate(device); });
    return devices;
}

} // namespace sony::transport
