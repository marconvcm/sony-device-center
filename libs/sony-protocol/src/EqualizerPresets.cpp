#include "sony/protocol/EqualizerPresets.h"

#include <algorithm>
#include <cctype>

namespace sony::protocol {

namespace {

std::string toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

} // namespace

const std::vector<EqualizerPresetInfo>& equalizerPresets() noexcept {
    static const std::vector<EqualizerPresetInfo> kPresets{
        {EqualizerPreset::Off,         "off",          "Off"},
        {EqualizerPreset::Bright,      "bright",       "Bright"},
        {EqualizerPreset::Excited,     "excited",      "Excited"},
        {EqualizerPreset::Mellow,      "mellow",       "Mellow"},
        {EqualizerPreset::Relaxed,     "relaxed",      "Relaxed"},
        {EqualizerPreset::Vocal,       "vocal",        "Vocal"},
        {EqualizerPreset::TrebleBoost, "treble-boost", "Treble Boost"},
        {EqualizerPreset::BassBoost,   "bass-boost",   "Bass Boost"},
        {EqualizerPreset::Speech,      "speech",       "Speech"},
        {EqualizerPreset::Manual,      "manual",       "Manual"},
    };
    return kPresets;
}

std::string equalizerPresetName(int preset) {
    for (const auto& info : equalizerPresets()) {
        if (static_cast<int>(info.preset) == preset) {
            return std::string(info.displayName);
        }
    }
    return "Preset (" + std::to_string(preset) + ")";
}

std::string_view equalizerPresetId(int preset) noexcept {
    for (const auto& info : equalizerPresets()) {
        if (static_cast<int>(info.preset) == preset) {
            return info.id;
        }
    }
    return {};
}

int equalizerPresetFromName(std::string_view name) {
    const auto low = toLower(name);

    for (const auto& info : equalizerPresets()) {
        if (low == info.id || low == toLower(info.displayName)) {
            return static_cast<int>(info.preset);
        }
    }

    // Aliases kept for CLI convenience and backwards compatibility.
    if (low == "treble" || low == "treble_boost") return static_cast<int>(EqualizerPreset::TrebleBoost);
    if (low == "bass" || low == "bass_boost")     return static_cast<int>(EqualizerPreset::BassBoost);

    try {
        return std::stoi(std::string(name));
    } catch (...) {
        return -1;
    }
}

} // namespace sony::protocol
