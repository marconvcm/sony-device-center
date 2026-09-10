#pragma once

#include "SingleInstanceFuture.h"
#include "BluetoothWrapper.h"
#include "Constants.h"
#include "sony/protocol/DeviceState.h"

#include <mutex>
#include <memory>

template <class T>
struct Property {
	T current;
	T desired;

	void fulfill();
	bool isFulfilled();
};

class Headphones {
public:
	Headphones(BluetoothWrapper& conn);

	void setAmbientSoundControl(bool val);
	bool getAmbientSoundControl();

	bool isFocusOnVoiceAvailable();
	void setFocusOnVoice(bool val);
	bool getFocusOnVoice();

	bool isSetAsmLevelAvailable();
	void setAsmLevel(int val);
	int getAsmLevel();

	void setSurroundPosition(SOUND_POSITION_PRESET val);
	SOUND_POSITION_PRESET getSurroundPosition();

	void setVptType(int val);
	int getVptType();

	// Handshake the device expects before it answers inquiry (GET) commands on the v2 protocol.
	void initDevice();

	// Battery (v2 inquiry). getBatteryLevel() returns -1 until requestBattery() succeeds.
	// For TWS earbuds requestBattery() also fills the per-earbud + case levels (hasDualBattery()).
	void requestBattery();
	int getBatteryLevel();
	bool isBatteryCharging();
	bool hasDualBattery();
	int getBatteryLeft();   // -1 if n/a
	int getBatteryRight();
	int getBatteryCase();

	// Equalizer (v2). requestEqualizer() reads current state; setEqualizerPreset() pushes a preset immediately.
	void requestEqualizer();
	EQ_PRESET getEqualizerPreset();
	void setEqualizerPreset(EQ_PRESET preset);
	// Custom (manual) EQ: 5 band values + clear bass, each in [-10, 10]. Selects the MANUAL preset.
	void setEqualizerCustom(int clearBass, const std::vector<int>& bands);
	int getClearBass();
	int getEqualizerBand(int index); // 0..4

	// DSEE / audio upsampling (v2).
	void requestDsee();
	bool getDsee();
	void setDsee(bool enabled);

	// Reads the device's current ambient/NC state into the *current* properties (for live polling so
	// changes made with the headphone's own button are reflected in the app).
	void requestAmbientState();

	// Optional features. probeCapabilities() sends each GET on connect; a feature is "supported" only if
	// the device answers (otherwise we must never send its SET - see the WH-1000XM4 power-off incident).
	void probeCapabilities();

	bool hasAutoPowerOff();
	int getAutoPowerOff();               // index into the option list (0=Off,1=5m,2=30m,3=1h,4=3h,5=when taken off)
	void setAutoPowerOff(int index);

	bool hasFirmware();
	std::string getFirmware();
	bool hasCodec();
	std::string getCodec();

	bool hasSpeakToChat();
	bool getSpeakToChat();
	void setSpeakToChat(bool enabled);

	bool hasAdaptiveVolume();
	bool getAdaptiveVolume();
	void setAdaptiveVolume(bool enabled);

	bool isChanged();
	void setChanges();

	// Phase 10 & 11: Immutable device state snapshots
	[[nodiscard]] sony::protocol::DeviceState state() const;
	[[nodiscard]] std::shared_ptr<const sony::protocol::DeviceState> snapshot() const;

private:
	Property<bool> _ambientSoundControl = { 0 };
	Property<bool> _focusOnVoice = { 0 };
	Property<int> _asmLevel = { 0 };
	Property<SOUND_POSITION_PRESET> _surroundPosition = { SOUND_POSITION_PRESET::OUT_OF_RANGE, SOUND_POSITION_PRESET::OFF };
	Property<int> _vptType = { 0 };

	// Structured device state replacing scattered primitive fields
	sony::protocol::DeviceState _deviceState;

	bool _hasAutoPowerOff = false;
	bool _hasFirmware = false;
	bool _hasCodec = false;
	bool _hasSpeakToChat = false;
	bool _hasAdaptiveVolume = false;

	mutable std::mutex _propertyMtx;

	BluetoothWrapper& _conn;
};

template<class T>
inline void Property<T>::fulfill()
{
	this->current = this->desired;
}

template<class T>
inline bool Property<T>::isFulfilled()
{
	return this->desired == this->current;
}
