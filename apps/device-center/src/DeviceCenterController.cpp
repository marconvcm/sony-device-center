#include "DeviceCenterController.h"
#include "I18nManager.h"
#include "sony/core/DeviceService.h"
#include "sony/core/IpcProtocol.h"
#include "sony/protocol/DeviceProfileRegistry.h"
#include "sony/transport/PlatformTransport.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>
#include <sstream>

namespace sony::devicecenter {

DeviceCenterController::DeviceCenterController(QObject* parent)
    : QObject(parent), _ipcClient(std::make_unique<core::IpcClient>()) {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    _currentLanguage = settings.value("language", "en").toString();
    _initService();
    refreshDiscoveredDevices();
}

DeviceCenterController::~DeviceCenterController() = default;

void DeviceCenterController::_initService() {
    if (_ipcClient && _ipcClient->isDaemonRunning()) {
        _usingIpc = true;
        _syncState();
        return;
    }

    _usingIpc = false;
    std::shared_ptr<transport::ITransport> transport = transport::createPlatformTransport();
    std::shared_ptr<transport::IDeviceDiscovery> discovery = transport::createPlatformDiscovery();
    _directService = std::make_shared<core::DeviceService>(transport, discovery);


    auto devs = _directService->discoverDevices();
    if (!devs.empty()) {
        _deviceName = QString::fromStdString(devs.front().name);
        _deviceAddress = QString::fromStdString(devs.front().address);
        try {
            _directService->connect(transport::DeviceAddress(devs.front().address), devs.front().name);
            _connected = true;
        } catch (...) {
            _connected = false;
        }
    } else {
        _connected = false;
        _deviceName = "No Sony Device Connected";
        _deviceAddress = "";
        _batteryLevel = 0;
    }
    _syncState();
}

void DeviceCenterController::_syncState() {
    if (_usingIpc && _ipcClient && _ipcClient->isDaemonRunning()) {
        auto statusResp = _ipcClient->sendCommand("status");
        _connected = statusResp.success;

        auto infoResp = _ipcClient->sendCommand("info");
        if (infoResp.success && !infoResp.data.empty()) {
            _connected = true;
            std::istringstream iss(infoResp.data);
            std::string firstLine;
            if (std::getline(iss, firstLine) && !firstLine.empty()) {
                _deviceName = QString::fromStdString(firstLine);
            }
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find("Noise Cancelling") != std::string::npos) {
                    _noiseControlMode = "cancelling";
                } else if (line.find("Ambient") != std::string::npos) {
                    _noiseControlMode = "ambient";
                } else if (line.find("Off") != std::string::npos && line.find("Noise Control") == std::string::npos) {
                    _noiseControlMode = "off";
                }
            }
        }

        auto batResp = _ipcClient->sendCommand("battery");
        if (batResp.success && !batResp.data.empty()) {
            auto s = batResp.data;
            auto pctPos = s.find('%');
            if (pctPos != std::string::npos) {
                size_t start = s.rfind(' ', pctPos);
                if (start != std::string::npos) {
                    try {
                        _batteryLevel = std::stoi(s.substr(start + 1, pctPos - start - 1));
                    } catch (...) {}
                }
            }
            _isCharging = (s.find("Charging") != std::string::npos && s.find("Discharging") == std::string::npos);
        }

        auto eqResp = _ipcClient->sendCommand("eq get");
        if (eqResp.success && !eqResp.data.empty()) {
            if (eqResp.data.find("Preset: ") != std::string::npos) {
                auto pName = eqResp.data.substr(eqResp.data.find("Preset: ") + 8);
                auto endLine = pName.find('\n');
                if (endLine != std::string::npos) pName = pName.substr(0, endLine);
                _equalizerPresetName = QString::fromStdString(pName);
            }
        }
    } else if (_directService && _directService->activeDevice() && _directService->isConnected()) {
        auto snap = _directService->snapshot();
        _connected = true;
        _batteryLevel = snap->battery.main.value_or(0);
        _isCharging = snap->battery.charging;
        if (snap->noiseControl.mode == protocol::NoiseControlMode::NoiseCancelling) {
            _noiseControlMode = "cancelling";
        } else if (snap->noiseControl.mode == protocol::NoiseControlMode::Ambient) {
            _noiseControlMode = "ambient";
        } else {
            _noiseControlMode = "off";
        }
        _ambientLevel = snap->noiseControl.ambientLevel > 0 ? snap->noiseControl.ambientLevel : 10;
        _focusOnVoice = snap->noiseControl.focusOnVoice;
        _equalizerPreset = snap->equalizer.preset;
        _clearBass = snap->equalizer.clearBass;
        _dsee = snap->dsee;
        _speakToChat = snap->speakToChat;
        _adaptiveVolume = snap->adaptiveVolume;
        _autoPowerOff = snap->autoPowerOff;
    } else {
        _connected = false;
        _batteryLevel = 0;
        _isCharging = false;
        _noiseControlMode = "off";
    }
    emit stateChanged();
    emit capabilitiesChanged();
}

QString DeviceCenterController::deviceName() const { return _deviceName; }
QString DeviceCenterController::deviceAddress() const { return _deviceAddress; }
bool DeviceCenterController::isConnected() const { return _connected; }
int DeviceCenterController::batteryLevel() const { return _batteryLevel; }
bool DeviceCenterController::isCharging() const { return _isCharging; }
QString DeviceCenterController::noiseControlMode() const { return _noiseControlMode; }
int DeviceCenterController::ambientLevel() const { return _ambientLevel; }
bool DeviceCenterController::focusOnVoice() const { return _focusOnVoice; }
int DeviceCenterController::equalizerPreset() const { return _equalizerPreset; }
QString DeviceCenterController::equalizerPresetName() const { return _equalizerPresetName; }
int DeviceCenterController::clearBass() const { return _clearBass; }
QVariantList DeviceCenterController::equalizerBands() const { return _equalizerBands; }
bool DeviceCenterController::dsee() const { return _dsee; }
bool DeviceCenterController::speakToChat() const { return _speakToChat; }
bool DeviceCenterController::adaptiveVolume() const { return _adaptiveVolume; }
int DeviceCenterController::autoPowerOff() const { return _autoPowerOff; }

QString DeviceCenterController::heroImagePath() const {
    QString lower = _deviceName.toLower();
    if (lower.contains("wf") && lower.contains("xm6")) {
        return "resources/devices/wf-1000xm6.png";
    } else if (lower.contains("wf") && lower.contains("xm5")) {
        return "resources/devices/wf-1000xm5.png";
    } else if (lower.contains("wf") && lower.contains("xm4")) {
        return "resources/devices/wf-1000xm4.png";
    } else if (lower.contains("wf") && lower.contains("xm3")) {
        return "resources/devices/wf-1000xm3.png";
    } else if (lower.contains("xm6")) {
        return "resources/devices/wh-1000xm6.png";
    } else if (lower.contains("xm5")) {
        return "resources/devices/wh-1000xm5.png";
    } else if (lower.contains("xm4")) {
        return "resources/devices/wh-1000xm4.png";
    } else if (lower.contains("xm3")) {
        return "resources/devices/wh-1000xm3.png";
    } else if (lower.contains("ch720")) {
        return "resources/devices/wh-ch720n.png";
    } else if (lower.contains("linkbuds")) {
        return "resources/devices/linkbuds-s.png";
    }
    return "resources/devices/wh-1000xm5.png";
}

bool DeviceCenterController::hasAnc() const {
    if (_directService && _directService->activeDevice()) {
        return _directService->activeDevice()->capabilities().noiseCancelling;
    }
    auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(_deviceName.toStdString());
    return profile.capabilities.noiseCancelling;
}

bool DeviceCenterController::hasAmbient() const {
    if (_directService && _directService->activeDevice()) {
        return _directService->activeDevice()->capabilities().ambientSound;
    }
    auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(_deviceName.toStdString());
    return profile.capabilities.ambientSound;
}

bool DeviceCenterController::hasEqualizer() const {
    if (_directService && _directService->activeDevice()) {
        return _directService->activeDevice()->capabilities().equalizer;
    }
    auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(_deviceName.toStdString());
    return profile.capabilities.equalizer;
}

bool DeviceCenterController::hasClearBass() const {
    if (_directService && _directService->activeDevice()) {
        return _directService->activeDevice()->capabilities().clearBass;
    }
    auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(_deviceName.toStdString());
    return profile.capabilities.clearBass;
}

bool DeviceCenterController::hasDsee() const {
    if (_directService && _directService->activeDevice()) {
        return _directService->activeDevice()->capabilities().dsee;
    }
    auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(_deviceName.toStdString());
    return profile.capabilities.dsee;
}

bool DeviceCenterController::hasSpeakToChat() const {
    if (_directService && _directService->activeDevice()) {
        return _directService->activeDevice()->capabilities().speakToChat;
    }
    auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(_deviceName.toStdString());
    return profile.capabilities.speakToChat;
}

bool DeviceCenterController::hasAdaptiveVolume() const {
    if (_directService && _directService->activeDevice()) {
        return _directService->activeDevice()->capabilities().adaptiveVolume;
    }
    auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(_deviceName.toStdString());
    return profile.capabilities.adaptiveVolume;
}

QVariantList DeviceCenterController::pairedDevices() const { return _pairedDevices; }

void DeviceCenterController::setAnc(bool enabled) {
    _noiseControlMode = enabled ? "cancelling" : "off";
    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand(enabled ? "anc on" : "anc off");
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setAnc(enabled);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setAmbient(int level, bool focusOnVoice) {
    _noiseControlMode = "ambient";
    _ambientLevel = std::clamp(level, 1, 20);
    _focusOnVoice = focusOnVoice;
    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand("ambient " + std::to_string(_ambientLevel));
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setAmbient(_ambientLevel, _focusOnVoice);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setNoiseControlOff() {
    _noiseControlMode = "off";
    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand("anc off");
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setAnc(false);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setEqualizerPreset(int preset) {
    _equalizerPreset = preset;
    std::string presetStr = "off";
    switch (preset) {
        case 0x11: presetStr = "bright"; _equalizerPresetName = "Bright"; break;
        case 0x12: presetStr = "excited"; _equalizerPresetName = "Excited"; break;
        case 0x13: presetStr = "mellow"; _equalizerPresetName = "Mellow"; break;
        case 0x14: presetStr = "relaxed"; _equalizerPresetName = "Relaxed"; break;
        case 0x15: presetStr = "vocal"; _equalizerPresetName = "Vocal"; break;
        case 0x16: presetStr = "bass-boost"; _equalizerPresetName = "Bass Boost"; break;
        case 0x17: presetStr = "treble-boost"; _equalizerPresetName = "Treble Boost"; break;
        case 0x18: presetStr = "speech"; _equalizerPresetName = "Speech"; break;
        case 0x01: presetStr = "custom1"; _equalizerPresetName = "Custom 1"; break;
        case 0x02: presetStr = "custom2"; _equalizerPresetName = "Custom 2"; break;
        default: presetStr = "off"; _equalizerPresetName = "Off"; break;
    }

    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand("eq preset " + presetStr);
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setEqualizerPreset(preset);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setEqualizerCustom(int clearBass, const QVariantList& bands) {
    _clearBass = std::clamp(clearBass, -10, 10);
    _equalizerBands = bands;
    _equalizerPreset = 0x01;
    _equalizerPresetName = "Custom";

    try {
        if (_usingIpc && _ipcClient) {
            std::ostringstream oss;
            oss << "eq custom " << _clearBass;
            for (const auto& b : bands) {
                oss << " " << b.toInt();
            }
            _ipcClient->sendCommand(oss.str());
        } else if (_directService && _directService->activeDevice()) {
            std::array<int, 5> bArr{0, 0, 0, 0, 0};
            for (int i = 0; i < 5 && i < bands.size(); ++i) {
                bArr[i] = bands[i].toInt();
            }
            _directService->activeDevice()->setEqualizerCustom(_clearBass, bArr);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setDsee(bool enabled) {
    _dsee = enabled;
    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand(enabled ? "dsee on" : "dsee off");
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setDsee(enabled);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setSpeakToChat(bool enabled) {
    _speakToChat = enabled;
    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand(enabled ? "speaktochat on" : "speaktochat off");
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setSpeakToChat(enabled);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setAdaptiveVolume(bool enabled) {
    _adaptiveVolume = enabled;
    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand(enabled ? "adaptive on" : "adaptive off");
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setAdaptiveVolume(enabled);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::setAutoPowerOff(int index) {
    _autoPowerOff = index;
    try {
        if (_usingIpc && _ipcClient) {
            _ipcClient->sendCommand("apo " + std::to_string(index));
        } else if (_directService && _directService->activeDevice()) {
            _directService->activeDevice()->setAutoPowerOff(index);
        }
    } catch (...) {}
    emit stateChanged();
}

void DeviceCenterController::connectDevice(const QString& address, const QString& name) {
    _deviceAddress = address;
    if (!name.isEmpty()) _deviceName = name;

    if (_directService) {
        try {
            _directService->connect(transport::DeviceAddress(address.toStdString()), _deviceName.toStdString());
            _connected = true;
        } catch (...) {
            _connected = false;
        }
    }
    _syncState();
    refreshDiscoveredDevices();
}

void DeviceCenterController::disconnectDevice() {
    _connected = false;
    if (_directService) {
        _directService->disconnect();
    }
    emit stateChanged();
    refreshDiscoveredDevices();
}

void DeviceCenterController::refreshDiscoveredDevices() {
    _pairedDevices.clear();
    std::vector<core::DiscoveredDevice> devs;
    if (_usingIpc && _ipcClient && _ipcClient->isDaemonRunning()) {
        auto resp = _ipcClient->sendCommand("devices");
        if (resp.success && !resp.data.empty()) {
            std::istringstream iss(resp.data);
            std::string line;
            while (std::getline(iss, line)) {
                auto openBracket = line.rfind('[');
                auto closeBracket = line.rfind(']');
                if (openBracket != std::string::npos && closeBracket != std::string::npos && closeBracket > openBracket) {
                    std::string name = line.substr(0, openBracket);
                    while (!name.empty() && name.back() == ' ') name.pop_back();
                    std::string mac = line.substr(openBracket + 1, closeBracket - openBracket - 1);
                    devs.push_back({.address = mac, .name = name});
                }
            }
        }
    } else if (_directService) {
        devs = _directService->discoverDevices();
    }

    for (const auto& d : devs) {
        QVariantMap item;
        item["name"] = QString::fromStdString(d.name);
        item["address"] = QString::fromStdString(d.address);
        bool isThisActive = (_connected && QString::fromStdString(d.address) == _deviceAddress);
        item["connected"] = isThisActive;
        _pairedDevices.append(item);
    }
    emit pairedDevicesChanged();
}

bool DeviceCenterController::autostart() const {
#if defined(Q_OS_LINUX)
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QString desktopFile = autostartDir + "/sony-device-center.desktop";
    return QFileInfo::exists(desktopFile);
#elif defined(Q_OS_WIN)
    QSettings bootSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return bootSettings.contains("SonyDeviceCenter");
#else
    return false;
#endif
}

void DeviceCenterController::setAutostart(bool enable) {
#if defined(Q_OS_LINUX)
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QString desktopFile = autostartDir + "/sony-device-center.desktop";
    if (enable) {
        QDir().mkpath(autostartDir);
        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=Sony Device Center\n";
            out << "Comment=Unofficial open-source companion for Sony WH/WF/LinkBuds audio devices\n";
            out << "Exec=" << QCoreApplication::applicationFilePath() << "\n";
            out << "Icon=sony-device-center\n";
            out << "Terminal=false\n";
            out << "Categories=Audio;AudioVideo;Settings;\n";
            out << "X-GNOME-Autostart-enabled=true\n";
        }
    } else {
        QFile::remove(desktopFile);
    }
#elif defined(Q_OS_WIN)
    QSettings bootSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enable) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        bootSettings.setValue("SonyDeviceCenter", "\"" + appPath + "\"");
    } else {
        bootSettings.remove("SonyDeviceCenter");
    }
#endif
    emit autostartChanged();
}

QString DeviceCenterController::appVersion() const {
    return QCoreApplication::applicationVersion();
}

QString DeviceCenterController::currentLanguage() const {
    return _currentLanguage;
}

QVariantList DeviceCenterController::availableLanguages() const {
    return I18nManager::instance().availableLanguages();
}

void DeviceCenterController::setLanguage(const QString& langCode) {
    if (_currentLanguage != langCode) {
        _currentLanguage = langCode;
        QSettings settings("SonyBridge", "SonyDeviceCenter");
        settings.setValue("language", langCode);
        emit languageChanged();
    }
}

QString DeviceCenterController::t(const QString& key) const {
    return I18nManager::instance().translate(key, _currentLanguage);
}

void DeviceCenterController::openUrl(const QString& url) {
    QDesktopServices::openUrl(QUrl(url));
}

} // namespace sony::devicecenter
