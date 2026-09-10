#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace sony::protocol {

enum class SonyProtocolVersion : signed char
{
    V1 = 0,
    V2 = 1
};

enum class SonyModel
{
    Unknown,

    WH1000XM3,
    WH1000XM4,
    WH1000XM5,
    WH1000XM6,

    WF1000XM4,
    WF1000XM5,
    WF1000XM6,

    WHCH720N,
    ULTWear,
    LinkBudsS
};

struct DeviceCapabilities
{
    bool battery = false;
    bool dualBattery = false;

    bool noiseCancelling = false;
    bool ambientSound = false;
    bool focusOnVoice = false;

    bool equalizer = false;
    bool clearBass = false;

    bool dsee = false;

    bool speakToChat = false;
    bool adaptiveVolume = false;

    bool autoPowerOff = false;

    bool firmwareInfo = false;
    bool codecInfo = false;

    bool wearSensor = false;

    bool multipoint = false;
};

struct DeviceProfile
{
    SonyModel model{SonyModel::Unknown};
    SonyProtocolVersion protocol{SonyProtocolVersion::V1};
    DeviceCapabilities capabilities{};
};

[[nodiscard]] constexpr std::string_view to_string(SonyModel model) noexcept {
    switch (model) {
        case SonyModel::WH1000XM3: return "WH-1000XM3";
        case SonyModel::WH1000XM4: return "WH-1000XM4";
        case SonyModel::WH1000XM5: return "WH-1000XM5";
        case SonyModel::WH1000XM6: return "WH-1000XM6";
        case SonyModel::WF1000XM4: return "WF-1000XM4";
        case SonyModel::WF1000XM5: return "WF-1000XM5";
        case SonyModel::WHCH720N:  return "WH-CH720N";
        case SonyModel::ULTWear:    return "ULT WEAR";
        case SonyModel::LinkBudsS:  return "LinkBuds S";
        case SonyModel::Unknown:   return "Unknown";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view to_string(SonyProtocolVersion version) noexcept {
    switch (version) {
        case SonyProtocolVersion::V1: return "V1";
        case SonyProtocolVersion::V2: return "V2";
    }
    return "Unknown";
}

} // namespace sony::protocol
