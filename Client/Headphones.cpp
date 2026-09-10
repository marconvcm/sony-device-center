#include "Headphones.h"
#include "CommandSerializer.h"

#include <stdexcept>
#include <utility>
#include <string>
#include <algorithm>

Headphones::Headphones(BluetoothWrapper& conn) : _conn(conn)
{
}

void Headphones::setAmbientSoundControl(bool val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_ambientSoundControl.desired = val;
}

bool Headphones::getAmbientSoundControl()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_ambientSoundControl.current;
}

bool Headphones::isFocusOnVoiceAvailable()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_ambientSoundControl.current && this->_asmLevel.current > MINIMUM_VOICE_FOCUS_STEP;
}

void Headphones::setFocusOnVoice(bool val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_focusOnVoice.desired = val;
}

bool Headphones::getFocusOnVoice()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_focusOnVoice.current;
}

bool Headphones::isSetAsmLevelAvailable()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_ambientSoundControl.current;
}

void Headphones::setAsmLevel(int val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_asmLevel.desired = val;
}

int Headphones::getAsmLevel()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_asmLevel.current;
}

void Headphones::setSurroundPosition(SOUND_POSITION_PRESET val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_surroundPosition.desired = val;
}

SOUND_POSITION_PRESET Headphones::getSurroundPosition()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_surroundPosition.current;
}

void Headphones::setVptType(int val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_vptType.desired = val;
}

int Headphones::getVptType()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_vptType.current;
}

void Headphones::initDevice()
{
	// v2 devices expect an init handshake before they answer GET inquiries. Best-effort: the reply
	// (INIT_REPLY, 8 bytes on v2) is ignored here - we only need the device to enter the ready state.
	this->_conn.sendCommandAndReadResponse(
		{ (char)V2Command::INIT_REQUEST, 0x00 },
		V2Command::INIT_REPLY
	);
}

void Headphones::requestBattery()
{
	// Single battery (over-ear and most models): GET 22 00 -> RET 23 00 <level> <charging>
	try {
		auto resp = this->_conn.sendCommandAndReadResponse({ (char)V2Command::BATTERY_GET, 0x00 }, V2Command::BATTERY_RET, 0x00);
		if (resp.size() >= 4) {
			std::lock_guard guard(this->_propertyMtx);
			this->_deviceState.battery.main = (unsigned char)resp[2];
			this->_deviceState.battery.charging = (resp[3] == 1);
			return; // found a single battery; don't waste time probing the TWS types
		}
	} catch (...) {}

	// TWS earbuds: dual L/R (22 09 -> 23 09 <Llvl> <Lchg> <Rlvl> <Rchg>) and case (22 0a -> 23 0a <lvl> <chg>).
	try {
		auto resp = this->_conn.sendCommandAndReadResponse({ (char)V2Command::BATTERY_GET, 0x09 }, V2Command::BATTERY_RET, 0x09);
		if (resp.size() >= 6) {
			std::lock_guard guard(this->_propertyMtx);
			unsigned char left = (unsigned char)resp[2];
			unsigned char right = (unsigned char)resp[4];
			this->_deviceState.battery.left = left;
			this->_deviceState.battery.right = right;
			this->_deviceState.battery.main = std::min(left, right);
			this->_deviceState.battery.charging = (resp[3] == 1) || (resp[5] == 1);
		}
	} catch (...) {}

	try {
		auto resp = this->_conn.sendCommandAndReadResponse({ (char)V2Command::BATTERY_GET, 0x0a }, V2Command::BATTERY_RET, 0x0a);
		if (resp.size() >= 4) {
			std::lock_guard guard(this->_propertyMtx);
			this->_deviceState.battery.caseBattery = (unsigned char)resp[2];
		}
	} catch (...) {}
}

int Headphones::getBatteryLevel()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.battery.main.value_or(-1);
}

bool Headphones::isBatteryCharging()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.battery.charging;
}

bool Headphones::hasDualBattery()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.battery.left.has_value() && this->_deviceState.battery.right.has_value();
}

int Headphones::getBatteryLeft()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.battery.left.value_or(-1);
}

int Headphones::getBatteryRight()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.battery.right.value_or(-1);
}

int Headphones::getBatteryCase()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.battery.caseBattery.value_or(-1);
}

void Headphones::requestEqualizer()
{
	// GET: 56 00  ->  RET: 57 00 <preset> 06 <bass+10> <b1..b5 +10>
	auto resp = this->_conn.sendCommandAndReadResponse(
		{ (char)V2Command::EQ_GET, 0x00 },
		V2Command::EQ_RET
	);
	if (resp.size() >= 3)
	{
		std::lock_guard guard(this->_propertyMtx);
		this->_deviceState.equalizer.preset = static_cast<int>((unsigned char)resp[2]);
		// When band data is present (57 00 <preset> 06 <bass+10> <b1..b5+10>), decode the -10..10 values.
		if (resp.size() >= 10)
		{
			this->_deviceState.equalizer.clearBass = (unsigned char)resp[4] - 10;
			for (size_t i = 0; i < 5; i++)
			{
				this->_deviceState.equalizer.bands[i] = (unsigned char)resp[5 + i] - 10;
			}
		}
	}
}

EQ_PRESET Headphones::getEqualizerPreset()
{
	std::lock_guard guard(this->_propertyMtx);
	return static_cast<EQ_PRESET>(this->_deviceState.equalizer.preset);
}

void Headphones::setEqualizerPreset(EQ_PRESET preset)
{
	// SET preset: 58 00 <preset> 00
	this->_conn.sendCommand({
		(char)V2Command::EQ_SET,
		0x00,
		(char)static_cast<unsigned char>(preset),
		0x00
	});
	std::lock_guard guard(this->_propertyMtx);
	this->_deviceState.equalizer.preset = static_cast<int>(preset);
}

void Headphones::setEqualizerCustom(int clearBass, const std::vector<int>& bands)
{
	// SET custom: 58 00 A0 06 <clearBass+10> <b1..b5 +10>  (0xA0 = MANUAL preset; values clamped to [-10,10])
	auto clamp = [](int v) { return (char)(unsigned char)(std::max(-10, std::min(10, v)) + 10); };
	Buffer cmd = {
		(char)V2Command::EQ_SET,
		0x00,
		(char)static_cast<unsigned char>(EQ_PRESET::MANUAL),
		0x06,
		clamp(clearBass)
	};
	for (int i = 0; i < 5; i++)
	{
		cmd.push_back(clamp(i < (int)bands.size() ? bands[i] : 0));
	}
	this->_conn.sendCommand(cmd);

	std::lock_guard guard(this->_propertyMtx);
	this->_deviceState.equalizer.preset = static_cast<int>(EQ_PRESET::MANUAL);
	this->_deviceState.equalizer.clearBass = clearBass;
	for (size_t i = 0; i < 5; i++)
	{
		this->_deviceState.equalizer.bands[i] = i < bands.size() ? bands[i] : 0;
	}
}

int Headphones::getClearBass()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.equalizer.clearBass;
}

int Headphones::getEqualizerBand(int index)
{
	std::lock_guard guard(this->_propertyMtx);
	return (index >= 0 && index < 5) ? this->_deviceState.equalizer.bands[index] : 0;
}

void Headphones::requestDsee()
{
	// GET: e6 01  ->  RET: e7 01 <enabled 0/1>
	auto resp = this->_conn.sendCommandAndReadResponse(
		{ (char)V2Command::DSEE_GET, 0x01 },
		V2Command::DSEE_RET
	);
	if (resp.size() >= 3)
	{
		std::lock_guard guard(this->_propertyMtx);
		this->_deviceState.dsee = resp[2] != 0;
	}
}

bool Headphones::getDsee()
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState.dsee;
}

void Headphones::setDsee(bool enabled)
{
	// SET: e8 01 <enabled 0/1>
	this->_conn.sendCommand({
		(char)V2Command::DSEE_SET,
		0x01,
		(char)(enabled ? 0x01 : 0x00)
	});
	std::lock_guard guard(this->_propertyMtx);
	this->_deviceState.dsee = enabled;
}

void Headphones::requestAmbientState()
{
	// The NCASM inquired-type differs by protocol generation (v2=0x17, v1=0x02), and so does the reply
	// layout. Sending the wrong one is what left v1 devices (WH-1000XM4) without valid handshake traffic.
	auto protocolVersion = this->_conn.getProtocolVersion();
	if (protocolVersion == SonyProtocolVersion::V2)
	{
		// GET: 66 17  ->  RET: 67 17 01 <effect> <settingType 0=NC/1=Ambient> <voice> <level>
		auto resp = this->_conn.sendCommandAndReadResponse({ 0x66, 0x17 }, 0x67);
		if (resp.size() >= 7)
		{
			bool on = resp[3] != 0;
			bool ambient = resp[4] != 0;
			bool voice = resp[5] != 0;
			int level = (unsigned char)resp[6];
			std::lock_guard guard(this->_propertyMtx);
			// Update both current and desired so the UI reflects reality and isChanged() stays false.
			this->_ambientSoundControl.current = this->_ambientSoundControl.desired = on;
			this->_asmLevel.current = this->_asmLevel.desired = ambient ? level : 0;
			this->_focusOnVoice.current = this->_focusOnVoice.desired = voice;

			this->_deviceState.noiseControl.mode = on ? (ambient ? sony::protocol::NoiseControlMode::Ambient : sony::protocol::NoiseControlMode::NoiseCancelling) : sony::protocol::NoiseControlMode::Off;
			this->_deviceState.noiseControl.ambientLevel = ambient ? level : 0;
			this->_deviceState.noiseControl.focusOnVoice = voice;
		}
	}
	else
	{
		// v1 (e.g. WH-1000XM4): upstream drove these settings fire-and-forget, with no read-back at all.
		// We still issue the GET because v1 devices need valid protocol traffic after connect to stay
		// powered on, and we read the reply to drain it from the socket - but we deliberately do NOT parse
		// it back into the NC/Ambient/level state. The v1 reply layout is unverified, and letting a
		// mis-parsed 2s poll overwrite the user's choice is exactly what made Ambient snap back to Noise
		// Cancelling and the level slider jump while being dragged. Trust the user's selection instead.
		this->_conn.sendCommandAndReadResponse({ 0x66, 0x02 }, 0x67);
	}
}

// --- Optional features (auto power off, firmware, codec, speak-to-chat, adaptive volume) ---

namespace
{
	// Auto-power-off 2-byte codes, indexed 0=Off,1=5min,2=30min,3=1h,4=3h,5=when-taken-off.
	const std::pair<unsigned char, unsigned char> APO_CODES[] = {
		{ 0x11, 0x00 }, { 0x00, 0x00 }, { 0x01, 0x01 }, { 0x02, 0x02 }, { 0x03, 0x03 }, { 0x10, 0x00 }
	};

	int apoIndexFromCode(unsigned char c0, unsigned char c1)
	{
		for (int i = 0; i < 6; i++)
		{
			if (APO_CODES[i].first == c0 && APO_CODES[i].second == c1) return i;
		}
		return 0;
	}

	std::string codecName(unsigned char code)
	{
		switch (code)
		{
			case 0x01: return "SBC";
			case 0x02: return "AAC";
			case 0x10: return "LDAC";
			case 0x20: return "aptX";
			case 0x21: return "aptX HD";
			default:   return "";
		}
	}
}

void Headphones::probeCapabilities()
{
	// Each GET is best-effort: an unsupported feature times out (recv timeout -> throw) and stays unsupported,
	// so we never expose or send a command the device can't handle.
	try {
		auto r = this->_conn.sendCommandAndReadResponse({ (char)V2Command::FW_GET, 0x02 }, V2Command::FW_RET);
		if (r.size() > 3) {
			std::lock_guard g(this->_propertyMtx);
			this->_deviceState.firmware = std::string(r.begin() + 3, r.end());
			this->_hasFirmware = true;
		}
	} catch (...) {}

	try {
		auto r = this->_conn.sendCommandAndReadResponse({ (char)V2Command::CODEC_GET, 0x02 }, V2Command::CODEC_RET);
		if (r.size() >= 3) {
			auto n = codecName((unsigned char)r[2]);
			if (!n.empty()) {
				std::lock_guard g(this->_propertyMtx);
				this->_deviceState.codec = n;
				this->_hasCodec = true;
			}
		}
	} catch (...) {}

	try {
		auto r = this->_conn.sendCommandAndReadResponse({ (char)V2Command::APO_GET, 0x05 }, V2Command::APO_RET);
		if (r.size() >= 4) {
			std::lock_guard g(this->_propertyMtx);
			this->_deviceState.autoPowerOff = apoIndexFromCode((unsigned char)r[2], (unsigned char)r[3]);
			this->_hasAutoPowerOff = true;
		}
	} catch (...) {}

	try {
		auto r = this->_conn.sendCommandAndReadResponse({ (char)V2Command::BTNMODE_GET, (char)V2Command::SUB_ADAPTIVE_VOLUME }, V2Command::BTNMODE_RET, V2Command::SUB_ADAPTIVE_VOLUME);
		if (r.size() >= 3) {
			std::lock_guard g(this->_propertyMtx);
			this->_deviceState.adaptiveVolume = (r[2] == 0);
			this->_hasAdaptiveVolume = true;
		}
	} catch (...) {}

	try {
		auto r = this->_conn.sendCommandAndReadResponse({ (char)V2Command::BTNMODE_GET, (char)V2Command::SUB_SPEAK_TO_CHAT }, V2Command::BTNMODE_RET, V2Command::SUB_SPEAK_TO_CHAT);
		if (r.size() >= 3) {
			std::lock_guard g(this->_propertyMtx);
			this->_deviceState.speakToChat = (r[2] == 0);
			this->_hasSpeakToChat = true;
		}
	} catch (...) {}
}

bool Headphones::hasAutoPowerOff() { std::lock_guard g(this->_propertyMtx); return this->_hasAutoPowerOff; }
int Headphones::getAutoPowerOff() { std::lock_guard g(this->_propertyMtx); return this->_deviceState.autoPowerOff; }
void Headphones::setAutoPowerOff(int index)
{
	if (index < 0 || index > 5) return;
	this->_conn.sendCommand({ (char)V2Command::APO_SET, 0x05, (char)APO_CODES[index].first, (char)APO_CODES[index].second });
	std::lock_guard guard(this->_propertyMtx);
	this->_deviceState.autoPowerOff = index;
}

bool Headphones::hasFirmware() { std::lock_guard g(this->_propertyMtx); return this->_hasFirmware; }
std::string Headphones::getFirmware() { std::lock_guard g(this->_propertyMtx); return this->_deviceState.firmware; }
bool Headphones::hasCodec() { std::lock_guard g(this->_propertyMtx); return this->_hasCodec; }
std::string Headphones::getCodec() { std::lock_guard g(this->_propertyMtx); return this->_deviceState.codec; }

bool Headphones::hasSpeakToChat() { std::lock_guard g(this->_propertyMtx); return this->_hasSpeakToChat; }
bool Headphones::getSpeakToChat() { std::lock_guard g(this->_propertyMtx); return this->_deviceState.speakToChat; }
void Headphones::setSpeakToChat(bool enabled)
{
	// SET: f8 0c <enabled? 0:1> 01  (enable bit is inverted on v2)
	this->_conn.sendCommand({ (char)V2Command::BTNMODE_SET, (char)V2Command::SUB_SPEAK_TO_CHAT, (char)(enabled ? 0x00 : 0x01), 0x01 });
	std::lock_guard guard(this->_propertyMtx);
	this->_deviceState.speakToChat = enabled;
}

bool Headphones::hasAdaptiveVolume() { std::lock_guard g(this->_propertyMtx); return this->_hasAdaptiveVolume; }
bool Headphones::getAdaptiveVolume() { std::lock_guard g(this->_propertyMtx); return this->_deviceState.adaptiveVolume; }
void Headphones::setAdaptiveVolume(bool enabled)
{
	// SET: f8 0a <enabled? 0:1>  (inverted)
	this->_conn.sendCommand({ (char)V2Command::BTNMODE_SET, (char)V2Command::SUB_ADAPTIVE_VOLUME, (char)(enabled ? 0x00 : 0x01) });
	std::lock_guard guard(this->_propertyMtx);
	this->_deviceState.adaptiveVolume = enabled;
}

bool Headphones::isChanged()
{
	std::lock_guard guard(this->_propertyMtx);
	return !(this->_ambientSoundControl.isFulfilled() && this->_asmLevel.isFulfilled() && this->_focusOnVoice.isFulfilled() && this->_surroundPosition.isFulfilled() && this->_vptType.isFulfilled());
}

void Headphones::setChanges()
{
	auto protocolVersion = this->_conn.getProtocolVersion();

	if (!(this->_ambientSoundControl.isFulfilled() && this->_focusOnVoice.isFulfilled() && this->_asmLevel.isFulfilled()))
	{
		if (protocolVersion == SonyProtocolVersion::V2)
		{
			// v2 has no separate "disabled" level - NC vs Ambient Sound is its own field, and level is 1-based.
			char asmLevel = static_cast<char>(this->_asmLevel.desired > 0 ? this->_asmLevel.desired : 1);

			this->_conn.sendCommand(CommandSerializer::serializeNcAndAsmSettingV2(
				this->_ambientSoundControl.desired ? NC_ASM_EFFECT::ON : NC_ASM_EFFECT::OFF,
				this->_asmLevel.desired > 0 ? NC_ASM_SETTING_TYPE_V2::AMBIENT_SOUND : NC_ASM_SETTING_TYPE_V2::NOISE_CANCELLING,
				this->_focusOnVoice.desired ? ASM_ID::VOICE : ASM_ID::NORMAL,
				asmLevel
			));
		}
		else
		{
			auto ncAsmEffect = this->_ambientSoundControl.desired ? NC_ASM_EFFECT::ADJUSTMENT_COMPLETION : NC_ASM_EFFECT::OFF;
			auto asmId = this->_focusOnVoice.desired ? ASM_ID::VOICE : ASM_ID::NORMAL;
			auto asmLevel = this->_ambientSoundControl.desired ? this->_asmLevel.desired : ASM_LEVEL_DISABLED;

			this->_conn.sendCommand(CommandSerializer::serializeNcAndAsmSetting(
				ncAsmEffect,
				NC_ASM_SETTING_TYPE::LEVEL_ADJUSTMENT,
				ASM_SETTING_TYPE::LEVEL_ADJUSTMENT,
				asmId,
				asmLevel
			));
		}

		std::lock_guard guard(this->_propertyMtx);
		this->_ambientSoundControl.fulfill();
		this->_asmLevel.fulfill();
		this->_focusOnVoice.fulfill();
		this->_deviceState.noiseControl.mode = this->_ambientSoundControl.current ? (this->_asmLevel.current > 0 ? sony::protocol::NoiseControlMode::Ambient : sony::protocol::NoiseControlMode::NoiseCancelling) : sony::protocol::NoiseControlMode::Off;
		this->_deviceState.noiseControl.ambientLevel = this->_asmLevel.current;
		this->_deviceState.noiseControl.focusOnVoice = this->_focusOnVoice.current;
	}

	// VPT/Surround has no known v2 command and no hardware on v2-only devices like the CH720N - v1 only.
	if (protocolVersion == SonyProtocolVersion::V1 && !(this->_vptType.isFulfilled() && this->_surroundPosition.isFulfilled())) {
		VPT_INQUIRED_TYPE command;
		unsigned char preset;

		if (this->_vptType.desired != 0) {
			command = VPT_INQUIRED_TYPE::VPT;
			preset = static_cast<unsigned char>(this->_vptType.desired);
		}
		else if (this->_surroundPosition.desired != SOUND_POSITION_PRESET::OFF) {
			command = VPT_INQUIRED_TYPE::SOUND_POSITION;
			preset = static_cast<unsigned char>(this->_surroundPosition.desired);
		}
		else {
			// Just used that one because it seems like it disables both
			if (this->_surroundPosition.current != SOUND_POSITION_PRESET::OFF) {
				command = VPT_INQUIRED_TYPE::SOUND_POSITION;
				preset = static_cast<unsigned char>(SOUND_POSITION_PRESET::OFF);
			}
			else if (this->_vptType.current != 0) {
				command = VPT_INQUIRED_TYPE::VPT;
				preset = 0;
			}
			else {
				throw std::logic_error("it's impossible that both values were changed to zero and were also previously zero");
			}
		}

		this->_conn.sendCommand(CommandSerializer::serializeVPTSetting(command, preset));

		std::lock_guard guard(this->_propertyMtx);
		this->_vptType.fulfill();
		this->_surroundPosition.fulfill();
	}
}

sony::protocol::DeviceState Headphones::state() const
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_deviceState;
}

std::shared_ptr<const sony::protocol::DeviceState> Headphones::snapshot() const
{
	std::lock_guard guard(this->_propertyMtx);
	return std::make_shared<const sony::protocol::DeviceState>(this->_deviceState);
}

uint64_t Headphones::onStateChanged(StateCallback callback)
{
	return this->_eventDispatcher.onStateChanged(std::move(callback));
}

uint64_t Headphones::onBatteryChanged(BatteryCallback callback)
{
	return this->_eventDispatcher.onBatteryChanged(std::move(callback));
}

uint64_t Headphones::onNoiseControlChanged(NoiseControlCallback callback)
{
	return this->_eventDispatcher.onNoiseControlChanged(std::move(callback));
}

uint64_t Headphones::onEqualizerChanged(EqualizerCallback callback)
{
	return this->_eventDispatcher.onEqualizerChanged(std::move(callback));
}

uint64_t Headphones::onConnectionChanged(ConnectionCallback callback)
{
	return this->_eventDispatcher.onConnectionChanged(std::move(callback));
}

void Headphones::removeEventListener(uint64_t subscriptionId)
{
	this->_eventDispatcher.removeListener(subscriptionId);
}

bool Headphones::handleNotification(const std::vector<uint8_t>& payload)
{
	std::lock_guard guard(this->_propertyMtx);
	bool handled = this->_eventDispatcher.parseNotificationPayload(payload, this->_deviceState);
	if (handled)
	{
		if (this->_deviceState.noiseControl.mode != sony::protocol::NoiseControlMode::Off)
		{
			this->_ambientSoundControl.current = this->_ambientSoundControl.desired = true;
			this->_asmLevel.current = this->_asmLevel.desired = this->_deviceState.noiseControl.ambientLevel;
			this->_focusOnVoice.current = this->_focusOnVoice.desired = this->_deviceState.noiseControl.focusOnVoice;
		}
		else
		{
			this->_ambientSoundControl.current = this->_ambientSoundControl.desired = false;
			this->_asmLevel.current = this->_asmLevel.desired = 0;
			this->_focusOnVoice.current = this->_focusOnVoice.desired = false;
		}
	}
	return handled;
}
