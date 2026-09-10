#include "sony/core/IpcProtocol.h"
#include "sony/protocol/EqualizerPresets.h"
#include <algorithm>
#include <sstream>

namespace sony::core {

namespace {

std::vector<std::string> splitTokens(std::string_view str) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : str) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) {
                tokens.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        tokens.push_back(std::move(cur));
    }
    return tokens;
}

std::string toLower(std::string_view s) {
    std::string res(s);
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return res;
}

int parsePresetName(std::string_view name) {
    return protocol::equalizerPresetFromName(name);
}

std::string presetToString(int preset) {
    return protocol::equalizerPresetName(preset);
}

} // namespace

IpcCommand IpcProtocol::parseCommand(std::string_view line) {
    IpcCommand cmd;
    cmd.raw = std::string(line);
    auto tokens = splitTokens(line);
    if (tokens.empty()) {
        return cmd;
    }

    std::string verb = toLower(tokens[0]);
    if (verb == "devices") {
        cmd.type = IpcCommandType::Devices;
    } else if (verb == "info") {
        cmd.type = IpcCommandType::Info;
    } else if (verb == "battery") {
        cmd.type = IpcCommandType::Battery;
    } else if (verb == "anc") {
        cmd.type = IpcCommandType::Anc;
    } else if (verb == "ambient") {
        cmd.type = IpcCommandType::Ambient;
    } else if (verb == "eq") {
        if (tokens.size() > 1 && toLower(tokens[1]) == "get") {
            cmd.type = IpcCommandType::EqGet;
        } else if (tokens.size() > 1 && toLower(tokens[1]) == "preset") {
            cmd.type = IpcCommandType::EqPreset;
        } else if (tokens.size() > 1 && toLower(tokens[1]) == "custom") {
            cmd.type = IpcCommandType::EqCustom;
        } else {
            cmd.type = IpcCommandType::EqPreset;
        }
    } else if (verb == "dsee") {
        cmd.type = IpcCommandType::Dsee;
    } else if (verb == "autopoweroff" || verb == "apo") {
        cmd.type = IpcCommandType::AutoPowerOff;
    } else if (verb == "status") {
        cmd.type = IpcCommandType::Status;
    }

    if (tokens.size() > 1) {
        cmd.args.assign(tokens.begin() + 1, tokens.end());
    }

    return cmd;
}

std::string IpcProtocol::serializeResponse(const IpcResponse& response) {
    std::string encodedData = response.data;
    for (char& c : encodedData) {
        if (c == '\n') c = '\x1e';
    }
    std::ostringstream oss;
    oss << (response.success ? "OK" : "ERR") << "|"
        << response.message << "|"
        << encodedData << "\n";
    return oss.str();
}

IpcResponse IpcProtocol::parseResponse(std::string_view line) {
    IpcResponse resp;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    size_t firstPipe = line.find('|');
    if (firstPipe == std::string_view::npos) {
        resp.success = false;
        resp.message = std::string(line);
        return resp;
    }

    auto status = line.substr(0, firstPipe);
    resp.success = (status == "OK");

    size_t secondPipe = line.find('|', firstPipe + 1);
    if (secondPipe == std::string_view::npos) {
        resp.message = std::string(line.substr(firstPipe + 1));
    } else {
        resp.message = std::string(line.substr(firstPipe + 1, secondPipe - firstPipe - 1));
        std::string rawData = std::string(line.substr(secondPipe + 1));
        for (char& c : rawData) {
            if (c == '\x1e') c = '\n';
        }
        resp.data = std::move(rawData);
    }

    return resp;
}

IpcResponse IpcProtocol::execute(const IpcCommand& cmd, IDeviceService& service) {
    IpcResponse resp;

    if (cmd.type == IpcCommandType::Devices) {
        auto devs = service.discoverDevices();
        std::ostringstream oss;
        for (size_t i = 0; i < devs.size(); ++i) {
            oss << devs[i].name << " [" << devs[i].address << "]";
            if (i + 1 < devs.size()) oss << "\n";
        }
        resp.success = true;
        resp.message = std::to_string(devs.size()) + " devices found";
        resp.data = oss.str();
        return resp;
    }

    auto* dev = service.activeDevice();
    if (!dev || !dev->isConnected()) {
        resp.success = false;
        resp.message = "No device connected";
        return resp;
    }

    auto snap = dev->snapshot();

    try {
        switch (cmd.type) {
        case IpcCommandType::Info: {
            std::ostringstream oss;
            oss << dev->name() << "\n\n"
                << "Protocol: " << (dev->protocolVersion() == SonyProtocolVersion::V1 ? "v1" : "v2") << "\n"
                << "Firmware: " << (snap->firmware.empty() ? "Unknown" : snap->firmware) << "\n"
                << "Codec: " << (snap->codec.empty() ? "Unknown" : snap->codec) << "\n";
            if (snap->battery.main.has_value()) {
                oss << "Battery: " << *snap->battery.main << "%\n";
            }
            oss << "\nNoise Control:\n  ";
            if (snap->noiseControl.mode == protocol::NoiseControlMode::NoiseCancelling) {
                oss << "Noise Cancelling\n";
            } else if (snap->noiseControl.mode == protocol::NoiseControlMode::Ambient) {
                oss << "Ambient (Level " << snap->noiseControl.ambientLevel << ")\n";
            } else {
                oss << "Off\n";
            }

            oss << "\nCapabilities:\n";
            const auto& caps = dev->capabilities();
            if (caps.noiseCancelling) oss << "  ANC\n";
            if (caps.ambientSound) oss << "  Ambient Sound\n";
            if (caps.equalizer) oss << "  Equalizer\n";
            if (caps.clearBass) oss << "  Clear Bass\n";
            if (caps.dsee) oss << "  DSEE\n";
            if (caps.speakToChat) oss << "  Speak-to-Chat\n";
            if (caps.adaptiveVolume) oss << "  Adaptive Volume\n";

            resp.success = true;
            resp.message = dev->name();
            resp.data = oss.str();
            return resp;
        }

        case IpcCommandType::Battery: {
            std::ostringstream oss;
            if (snap->battery.left.has_value() && snap->battery.right.has_value()) {
                oss << "Left: " << *snap->battery.left << "%, Right: " << *snap->battery.right << "%";
                if (snap->battery.caseBattery.has_value()) {
                    oss << ", Case: " << *snap->battery.caseBattery << "%";
                }
            } else if (snap->battery.main.has_value()) {
                oss << "Battery: " << *snap->battery.main << "%";
            } else {
                oss << "Battery: Unknown";
            }
            if (snap->battery.charging) {
                oss << " (Charging)";
            }
            resp.success = true;
            resp.message = "Battery status";
            resp.data = oss.str();
            return resp;
        }

        case IpcCommandType::Anc: {
            bool on = true;
            if (!cmd.args.empty() && toLower(cmd.args[0]) == "off") {
                on = false;
            }
            dev->setAnc(on);
            resp.success = true;
            resp.message = on ? "ANC enabled" : "ANC disabled";
            return resp;
        }

        case IpcCommandType::Ambient: {
            if (!cmd.args.empty() && toLower(cmd.args[0]) == "off") {
                dev->setAnc(false);
                resp.success = true;
                resp.message = "Ambient sound disabled";
                return resp;
            }
            int level = 10;
            if (!cmd.args.empty()) {
                try {
                    level = std::clamp(std::stoi(cmd.args[0]), 1, 20);
                } catch (...) {
                    level = 10;
                }
            }
            dev->setAmbient(level, false);
            resp.success = true;
            resp.message = "Ambient sound set to level " + std::to_string(level);
            return resp;
        }

        case IpcCommandType::EqGet: {
            std::ostringstream oss;
            oss << "Preset: " << presetToString(snap->equalizer.preset) << "\n"
                << "Clear Bass: " << snap->equalizer.clearBass << "\n"
                << "Bands: [";
            for (size_t i = 0; i < snap->equalizer.bands.size(); ++i) {
                oss << snap->equalizer.bands[i];
                if (i + 1 < snap->equalizer.bands.size()) oss << ", ";
            }
            oss << "]";
            resp.success = true;
            resp.message = "Equalizer state";
            resp.data = oss.str();
            return resp;
        }

        case IpcCommandType::EqPreset: {
            std::string presetName = cmd.args.empty() ? "off" : cmd.args[0];
            if (presetName == "preset" && cmd.args.size() > 1) {
                presetName = cmd.args[1];
            }
            int preset = parsePresetName(presetName);
            if (preset < 0) {
                resp.success = false;
                resp.message = "Unknown preset: " + presetName;
                return resp;
            }
            dev->setEqualizerPreset(preset);
            resp.success = true;
            resp.message = "Equalizer set to preset " + presetToString(preset);
            return resp;
        }

        case IpcCommandType::EqCustom: {
            int clearBass = 0;
            std::array<int, 5> bands = {0, 0, 0, 0, 0};
            // args: [custom, clearBass, b1, b2, b3, b4, b5] or [clearBass, b1..b5]
            size_t startIdx = (!cmd.args.empty() && toLower(cmd.args[0]) == "custom") ? 1 : 0;
            if (cmd.args.size() > startIdx) {
                try {
                    clearBass = std::clamp(std::stoi(cmd.args[startIdx]), -10, 10);
                } catch (...) {}
            }
            for (size_t i = 0; i < 5 && startIdx + 1 + i < cmd.args.size(); ++i) {
                try {
                    bands[i] = std::clamp(std::stoi(cmd.args[startIdx + 1 + i]), -10, 10);
                } catch (...) {}
            }
            dev->setEqualizerCustom(clearBass, bands);
            resp.success = true;
            resp.message = "Custom EQ applied";
            return resp;
        }

        case IpcCommandType::Dsee: {
            bool on = true;
            if (!cmd.args.empty()) {
                auto arg = toLower(cmd.args[0]);
                if (arg == "off") on = false;
            }
            dev->setDsee(on);
            resp.success = true;
            resp.message = on ? "DSEE enabled" : "DSEE disabled";
            return resp;
        }

        case IpcCommandType::AutoPowerOff: {
            int idx = 0;
            if (!cmd.args.empty()) {
                try {
                    idx = std::clamp(std::stoi(cmd.args[0]), 0, 5);
                } catch (...) {}
            }
            dev->setAutoPowerOff(idx);
            resp.success = true;
            resp.message = "Auto power off set to index " + std::to_string(idx);
            return resp;
        }

        case IpcCommandType::Status: {
            resp.success = true;
            resp.message = "Connected";
            resp.data = "model=" + dev->name();
            return resp;
        }

        default:
            resp.success = false;
            resp.message = "Unknown command: " + cmd.raw;
            return resp;
        }
    } catch (const std::exception& ex) {
        resp.success = false;
        resp.message = "Command failed: " + std::string(ex.what());
        return resp;
    }
}

} // namespace sony::core
