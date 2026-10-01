#pragma once

#include "IProtocol.h"
#include "DeviceProfile.h"
#include "SonyProtocolSession.h"
#include <mutex>

namespace sony::protocol {

class ProtocolV2 : public IProtocol {
public:
    // tenBandEqualizer selects the newer 10-band equalizer wire format
    // (inquired type 0x04, no separate Clear Bass slot) used by devices such
    // as the WH-1000XM6, instead of the legacy 5-band + Clear Bass format
    // (inquired type 0x00) shared with ProtocolV1. See DeviceProfile.h and
    // issue #10. The noise-control layout is selected from the model profile.
    // detailedBattery collects L/R/case even when the aggregate reply succeeds.
    // bleNoiseControl uses XM6 type 0x19 writes with preserved adaptation bytes;
    // Classic XM6 control retains type 0x17 writes.
    explicit ProtocolV2(SonyProtocolSession& session, bool tenBandEqualizer = false,
                        NoiseControlLayout noiseControlLayout = NoiseControlLayout::Standard,
                        bool detailedBattery = false, bool bleNoiseControl = false);
    ~ProtocolV2() override = default;

    [[nodiscard]] ProtocolGeneration generation() const noexcept override {
        return ProtocolGeneration::V2;
    }

    void initDevice() override;

    // V2 uses opcode 0x22 for battery inquiries
    BatteryState getBattery() override;

    NoiseControlState getNoiseControl() override;
    void setNoiseControl(const NoiseControlState& state) override;

    EqualizerState getEqualizer() override;
    void setEqualizerPreset(int preset) override;
    void setEqualizerCustom(int clearBass, const std::vector<int>& bands) override;

    bool getDsee() override;
    void setDsee(bool enabled) override;

    std::string getFirmwareVersion() override;
    std::string getCodec() override;

    int getAutoPowerOff() override;
    void setAutoPowerOff(int index) override;

    bool getSpeakToChat() override;
    void setSpeakToChat(bool enabled) override;

    bool getAdaptiveVolume() override;
    void setAdaptiveVolume(bool enabled) override;

private:
    SonyProtocolSession& _session;
    std::mutex _mutex;
    bool _tenBandEqualizer;
    NoiseControlLayout _noiseControlLayout;
    bool _detailedBattery;
    bool _bleNoiseControl;
    SonyFrame _queryNoiseControl();
};

} // namespace sony::protocol
