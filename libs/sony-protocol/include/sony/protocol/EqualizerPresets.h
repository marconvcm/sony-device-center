#pragma once

// The single authoritative equalizer preset table.
//
// These byte values are reverse-engineered and were previously written out by
// hand in three places: the CLI, the IPC layer and the desktop controller. The
// controller's copy was off by one and had bass and treble swapped, so picking
// "Vocal" applied Relaxed, "Bright" fell through to Off, and the label under the
// chips named a different preset from the one in effect. Anything that needs to
// turn a preset into a name, or the reverse, uses this header.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sony::protocol {

enum class EqualizerPreset : uint8_t {
    Off         = 0x00,
    Bright      = 0x10,
    Excited     = 0x11,
    Mellow      = 0x12,
    Relaxed     = 0x13,
    Vocal       = 0x14,
    TrebleBoost = 0x15,
    BassBoost   = 0x16,
    Speech      = 0x17,
    Manual      = 0xa0,
};

struct EqualizerPresetInfo {
    EqualizerPreset preset;
    std::string_view id;           ///< stable identifier used on the CLI and over IPC
    std::string_view displayName;  ///< human-readable label
};

/// Every preset, in the order they are offered in the interface.
[[nodiscard]] const std::vector<EqualizerPresetInfo>& equalizerPresets() noexcept;

/// Display name for a raw preset byte. Unknown values render as "Preset (n)".
[[nodiscard]] std::string equalizerPresetName(int preset);

/// Stable identifier for a raw preset byte, e.g. "bass-boost". Empty if unknown.
[[nodiscard]] std::string_view equalizerPresetId(int preset) noexcept;

/// Parse an identifier or display name, case-insensitively. Accepts a few
/// aliases ("bass", "treble") and a decimal number. Returns -1 when unknown.
[[nodiscard]] int equalizerPresetFromName(std::string_view name);

} // namespace sony::protocol
