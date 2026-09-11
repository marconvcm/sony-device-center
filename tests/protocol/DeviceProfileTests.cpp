#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/DeviceProfileRegistry.h"

using namespace sony::protocol;

TEST_CASE("DeviceProfileRegistry: identifies model from advertised device names", "[protocol][profile]")
{
    REQUIRE(DeviceProfileRegistry::identifyModel("WH-1000XM3") == SonyModel::WH1000XM3);
    REQUIRE(DeviceProfileRegistry::identifyModel("Sony WH-1000XM3") == SonyModel::WH1000XM3);

    REQUIRE(DeviceProfileRegistry::identifyModel("WH-1000XM4") == SonyModel::WH1000XM4);
    REQUIRE(DeviceProfileRegistry::identifyModel("LE_WH-1000XM4") == SonyModel::WH1000XM4);
    REQUIRE(DeviceProfileRegistry::identifyModel("wh-1000xm4") == SonyModel::WH1000XM4);

    REQUIRE(DeviceProfileRegistry::identifyModel("WH-1000XM5") == SonyModel::WH1000XM5);
    REQUIRE(DeviceProfileRegistry::identifyModel("Sony WH-1000XM5") == SonyModel::WH1000XM5);

    REQUIRE(DeviceProfileRegistry::identifyModel("WH-1000XM6") == SonyModel::WH1000XM6);

    REQUIRE(DeviceProfileRegistry::identifyModel("WF-1000XM4") == SonyModel::WF1000XM4);
    REQUIRE(DeviceProfileRegistry::identifyModel("LE_WF-1000XM4") == SonyModel::WF1000XM4);

    REQUIRE(DeviceProfileRegistry::identifyModel("WF-1000XM5") == SonyModel::WF1000XM5);

    REQUIRE(DeviceProfileRegistry::identifyModel("WH-CH720N") == SonyModel::WHCH720N);
    REQUIRE(DeviceProfileRegistry::identifyModel("Sony WH-CH720N") == SonyModel::WHCH720N);

    REQUIRE(DeviceProfileRegistry::identifyModel("ULT WEAR") == SonyModel::ULTWear);
    REQUIRE(DeviceProfileRegistry::identifyModel("WH-ULT900N") == SonyModel::ULTWear);

    REQUIRE(DeviceProfileRegistry::identifyModel("LinkBuds S") == SonyModel::LinkBudsS);
    REQUIRE(DeviceProfileRegistry::identifyModel("WF-LS900N") == SonyModel::LinkBudsS);

    REQUIRE(DeviceProfileRegistry::identifyModel("Bose QC45") == SonyModel::Unknown);
    REQUIRE(DeviceProfileRegistry::identifyModel("") == SonyModel::Unknown);
}

TEST_CASE("DeviceProfileRegistry: provides immediate capabilities for known models", "[protocol][profile]")
{
    SECTION("WH-1000XM4 (V1)")
    {
        auto profile = DeviceProfileRegistry::getProfile(SonyModel::WH1000XM4);
        REQUIRE(profile.has_value());
        REQUIRE(profile->model == SonyModel::WH1000XM4);
        REQUIRE(profile->protocol == SonyProtocolVersion::V1);
        REQUIRE(profile->capabilities.battery == true);
        REQUIRE(profile->capabilities.dualBattery == false);
        REQUIRE(profile->capabilities.noiseCancelling == true);
        REQUIRE(profile->capabilities.ambientSound == true);
        REQUIRE(profile->capabilities.focusOnVoice == true);
        REQUIRE(profile->capabilities.equalizer == true);
        REQUIRE(profile->capabilities.clearBass == true);
        REQUIRE(profile->capabilities.firmwareInfo == true);
        REQUIRE(profile->capabilities.codecInfo == true);
        REQUIRE(profile->capabilities.dsee == false);
        REQUIRE(profile->capabilities.wearSensor == true);
        REQUIRE(profile->capabilities.multipoint == true);
    }

    SECTION("WH-1000XM5 (V2)")
    {
        auto profile = DeviceProfileRegistry::getProfile(SonyModel::WH1000XM5);
        REQUIRE(profile.has_value());
        REQUIRE(profile->model == SonyModel::WH1000XM5);
        REQUIRE(profile->protocol == SonyProtocolVersion::V2);
        REQUIRE(profile->capabilities.battery == true);
        REQUIRE(profile->capabilities.dualBattery == false);
        REQUIRE(profile->capabilities.noiseCancelling == true);
        REQUIRE(profile->capabilities.ambientSound == true);
        REQUIRE(profile->capabilities.equalizer == true);
        REQUIRE(profile->capabilities.clearBass == true);
        REQUIRE(profile->capabilities.dsee == true);
        REQUIRE(profile->capabilities.speakToChat == true);
        REQUIRE(profile->capabilities.adaptiveVolume == true);
        REQUIRE(profile->capabilities.autoPowerOff == true);
        REQUIRE(profile->capabilities.firmwareInfo == true);
        REQUIRE(profile->capabilities.codecInfo == true);
    }

    SECTION("WF-1000XM5 (TWS Earbuds, V2)")
    {
        auto profile = DeviceProfileRegistry::getProfile(SonyModel::WF1000XM5);
        REQUIRE(profile.has_value());
        REQUIRE(profile->model == SonyModel::WF1000XM5);
        REQUIRE(profile->protocol == SonyProtocolVersion::V2);
        REQUIRE(profile->capabilities.battery == true);
        REQUIRE(profile->capabilities.dualBattery == true); // Dual battery for TWS
        REQUIRE(profile->capabilities.wearSensor == true);
        REQUIRE(profile->capabilities.adaptiveVolume == true);
    }
}

TEST_CASE("DeviceProfileRegistry: handles unknown devices with fallback profile", "[protocol][profile]")
{
    auto profile = DeviceProfileRegistry::getProfileForDevice("Unknown Headset 9000");
    REQUIRE(profile.model == SonyModel::Unknown);
    REQUIRE_FALSE(DeviceProfileRegistry::isKnownDevice(profile.model));
    // Capabilities are all false for unknown devices so dynamic probing is allowed
    REQUIRE_FALSE(profile.capabilities.equalizer);
    REQUIRE_FALSE(profile.capabilities.dsee);
}

TEST_CASE("DeviceProfileRegistry: string conversions", "[protocol][profile]")
{
    REQUIRE(to_string(SonyModel::WH1000XM4) == "WH-1000XM4");
    REQUIRE(to_string(SonyModel::WH1000XM5) == "WH-1000XM5");
    REQUIRE(to_string(SonyModel::WHCH720N) == "WH-CH720N");
    REQUIRE(to_string(SonyModel::Unknown) == "Unknown");

    REQUIRE(to_string(SonyProtocolVersion::V1) == "V1");
    REQUIRE(to_string(SonyProtocolVersion::V2) == "V2");
}

// ---------------------------------------------------------------------------
// Equalizer preset table
//
// The desktop controller used to carry its own copy of this mapping, off by one
// with bass and treble swapped. These pin the values against the
// reverse-engineered reference in Client/Constants.h so a second copy cannot
// drift again.
#include "sony/protocol/EqualizerPresets.h"

TEST_CASE("Equalizer preset codes match the protocol reference", "[protocol][eq]") {
    CHECK(static_cast<int>(EqualizerPreset::Off)         == 0x00);
    CHECK(static_cast<int>(EqualizerPreset::Bright)      == 0x10);
    CHECK(static_cast<int>(EqualizerPreset::Excited)     == 0x11);
    CHECK(static_cast<int>(EqualizerPreset::Mellow)      == 0x12);
    CHECK(static_cast<int>(EqualizerPreset::Relaxed)     == 0x13);
    CHECK(static_cast<int>(EqualizerPreset::Vocal)       == 0x14);
    CHECK(static_cast<int>(EqualizerPreset::TrebleBoost) == 0x15);
    CHECK(static_cast<int>(EqualizerPreset::BassBoost)   == 0x16);
    CHECK(static_cast<int>(EqualizerPreset::Speech)      == 0x17);
    CHECK(static_cast<int>(EqualizerPreset::Manual)      == 0xa0);
}

TEST_CASE("Equalizer preset names round-trip", "[protocol][eq]") {
    for (const auto& info : equalizerPresets()) {
        const int code = static_cast<int>(info.preset);
        CHECK(equalizerPresetFromName(info.id) == code);
        CHECK(equalizerPresetFromName(info.displayName) == code);
        CHECK(equalizerPresetId(code) == info.id);
        CHECK(equalizerPresetName(code) == std::string(info.displayName));
    }
}

TEST_CASE("Equalizer preset lookup is case-insensitive and accepts aliases", "[protocol][eq]") {
    CHECK(equalizerPresetFromName("BASS-BOOST") == 0x16);
    CHECK(equalizerPresetFromName("bass") == 0x16);
    CHECK(equalizerPresetFromName("treble") == 0x15);
    CHECK(equalizerPresetFromName("Bass Boost") == 0x16);
    CHECK(equalizerPresetFromName("nonsense") == -1);
}
