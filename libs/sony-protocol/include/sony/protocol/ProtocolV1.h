#pragma once

#include "IProtocol.h"
#include "SonyProtocolSession.h"
#include <mutex>

namespace sony::protocol {

class ProtocolV1 : public IProtocol {
public:
    explicit ProtocolV1(SonyProtocolSession& session);
    ~ProtocolV1() override = default;

    [[nodiscard]] ProtocolGeneration generation() const noexcept override {
        return ProtocolGeneration::V1;
    }

    void initDevice() override;

    // Critical: V1 does not support battery queries over MDR.
    // Opcode 0x22 in V1 is Power Off, so getBattery() NEVER sends 0x22.
    BatteryState getBattery() override;

    NoiseControlState getNoiseControl() override;
    void setNoiseControl(const NoiseControlState& state) override;

    EqualizerState getEqualizer() override;
    void setEqualizerPreset(int preset) override;
    void setEqualizerCustom(int clearBass, const std::array<int, 5>& bands) override;

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

    // V1-specific surround & positioning commands
    void setVpt(int preset);
    void setSoundPosition(int preset);

private:
    SonyProtocolSession& _session;
    std::mutex _mutex;
    BatteryState _batteryState;
    NoiseControlState _noiseControlState;
};

} // namespace sony::protocol
