#include "sony/core/JsonProtocol.h"
#include "sony/protocol/EqualizerPresets.h"
#include <cctype>
namespace sony::core {
using Json = JsonProtocol::Json;
namespace {
template<class T> Json optional(const std::optional<T>& value) { return value ? Json(*value) : Json(nullptr); }
int integer(const Json& params, const char* name, int lo, int hi) {
    const auto& v = params.at(name);
    if (!v.is_number_integer() || v.get<int64_t>() < lo || v.get<int64_t>() > hi)
        throw std::invalid_argument(std::string(name) + " is out of range");
    return v.get<int>();
}
void supported(bool yes) { if (!yes) throw SonyException(SonyErrorCode::Unsupported, "Feature not supported by this device"); }
}
Json JsonProtocol::snapshot(IDeviceService& service) {
    const bool connected = service.isConnected();
    auto* dev = service.activeDevice();
    auto s = service.snapshot();
    const auto c = dev ? dev->capabilities() : protocol::DeviceCapabilities{};
    Json features = Json::object();
    for (const auto& [name, status] : s->features) {
        features[name] = {{"availability", !connected && status.availability == "valid" ? "stale" : status.availability},
            {"lastSuccessMs", status.lastSuccessMs}, {"error", status.error}};
    }
    return {
        {"connected", connected}, {"connectionState", service.connectionState()},
        {"address", service.selectedAddress()}, {"name", dev ? dev->name() : ""},
        {"lastError", service.lastError()}, {"protocol", dev ? std::string(protocol::to_string(dev->protocolVersion())) : "unknown"},
        {"capabilities", {{"anc", c.noiseCancelling}, {"ambient", c.ambientSound}, {"focusOnVoice", c.focusOnVoice},
            {"equalizer", c.equalizer}, {"clearBass", c.clearBass}, {"tenBandEqualizer", c.tenBandEqualizer},
            {"dsee", c.dsee}, {"battery", c.battery},
            {"speakToChat", c.speakToChat}, {"adaptiveVolume", c.adaptiveVolume}, {"autoPowerOff", c.autoPowerOff}}},
        {"features", features},
        {"battery", {{"main", optional(s->battery.main)}, {"left", optional(s->battery.left)}, {"right", optional(s->battery.right)},
            {"case", optional(s->battery.caseBattery)}, {"charging", s->battery.charging}}},
        {"noiseControl", {{"mode", s->noiseControl.mode == protocol::NoiseControlMode::NoiseCancelling ? "cancelling" :
            s->noiseControl.mode == protocol::NoiseControlMode::Ambient ? "ambient" : "off"},
            {"ambientLevel", s->noiseControl.ambientLevel}, {"focusOnVoice", s->noiseControl.focusOnVoice}}},
        {"equalizer", {{"preset", s->equalizer.preset}, {"presetName", protocol::equalizerPresetName(s->equalizer.preset)},
            {"clearBass", s->equalizer.clearBass}, {"bands", s->equalizer.bands}}},
        {"dsee", s->dsee}, {"speakToChat", s->speakToChat}, {"adaptiveVolume", s->adaptiveVolume},
        {"autoPowerOff", s->autoPowerOff}, {"codec", s->codec.empty() ? "Unknown" : s->codec},
        {"firmware", s->firmware.empty() ? "Unknown" : s->firmware}
    };
}
Json JsonProtocol::execute(const Json& request, IDeviceService& service) {
    Json response = {{"version", Version}, {"id", nullptr}, {"ok", false}};
    try {
        if (!request.is_object()) throw std::invalid_argument("Request must be an object");
        if (request.contains("id") && (request["id"].is_string() || request["id"].is_number_integer())) response["id"] = request["id"];
        else throw std::invalid_argument("Request id must be a string or integer");
        if (!request.contains("version") || request["version"] != Version) {
            response["error"] = {{"code", "VersionMismatch"}, {"message", "Update sonyd and Sony Device Center to matching versions"}};
            return response;
        }
        const auto method = request.at("method").get<std::string>();
        const auto params = request.value("params", Json::object());
        if (!params.is_object()) throw std::invalid_argument("params must be an object");
        Json data;
        if (method == "snapshot") data = snapshot(service);
        else if (method == "devices") {
            data = Json::array();
            for (const auto& d : service.discoverDevices())
                data.push_back({{"name", d.name}, {"address", d.address}, {"paired", optional(d.paired)}, {"systemConnected", optional(d.connected)}});
        } else if (method == "connect") {
            auto address = params.at("address").get<std::string>();
            if (address.size() != 17) throw std::invalid_argument("Expected a Bluetooth address in AA:BB:CC:DD:EE:FF format");
            for (size_t i = 0; i < address.size(); ++i) {
                if (i % 3 == 2 ? address[i] != ':' : !std::isxdigit(static_cast<unsigned char>(address[i])))
                    throw std::invalid_argument("Invalid Bluetooth address");
                address[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(address[i])));
            }
            service.connect(transport::DeviceAddress(address), params.value("name", std::string{}));
            data = snapshot(service);
        } else if (method == "disconnect") { service.disconnect(); data = snapshot(service); }
        else {
            auto* dev = service.activeDevice();
            if (!dev || !service.isConnected()) throw SonyException(SonyErrorCode::Disconnected, "Headphones are disconnected; wait for reconnection");
            const auto& c = dev->capabilities();
            if (method == "anc") { supported(c.noiseCancelling); dev->setAnc(params.at("enabled").get<bool>()); }
            else if (method == "ambient") {
                supported(c.ambientSound); const bool voice = params.value("focusOnVoice", false);
                if (voice) supported(c.focusOnVoice);
                dev->setAmbient(integer(params, "level", 1, 20), voice);
            } else if (method == "eqPreset") {
                supported(c.equalizer); const int preset = integer(params, "preset", 0, 255);
                if (protocol::equalizerPresetId(preset).empty()) throw std::invalid_argument("Unknown equalizer preset");
                dev->setEqualizerPreset(preset);
            } else if (method == "eqCustom") {
                supported(c.equalizer); const auto& values = params.at("bands");
                if (c.tenBandEqualizer) {
                    // 10-band devices (e.g. WH-1000XM6) have no separate
                    // Clear Bass slot; "bands" carries all ten raw device
                    // values directly (0-12, see issue #10).
                    if (!values.is_array() || values.size() != 10) throw std::invalid_argument("Expected ten equalizer bands");
                    std::vector<int> bands(10);
                    for (size_t i=0; i<10; ++i) bands[i] = integer(Json{{"value",values[i]}}, "value", 0, 12);
                    dev->setEqualizerCustom(0, bands);
                } else {
                    if (!values.is_array() || values.size() != 5) throw std::invalid_argument("Expected five equalizer bands");
                    std::vector<int> bands(5);
                    for (size_t i=0; i<5; ++i) bands[i] = integer(Json{{"value",values[i]}}, "value", -10, 10);
                    dev->setEqualizerCustom(integer(params, "clearBass", -10, 10), bands);
                }
            } else if (method == "dsee") { supported(c.dsee); dev->setDsee(params.at("enabled").get<bool>()); }
            else if (method == "speakToChat") { supported(c.speakToChat); dev->setSpeakToChat(params.at("enabled").get<bool>()); }
            else if (method == "adaptiveVolume") { supported(c.adaptiveVolume); dev->setAdaptiveVolume(params.at("enabled").get<bool>()); }
            else if (method == "autoPowerOff") { supported(c.autoPowerOff); dev->setAutoPowerOff(integer(params, "index", 0, 5)); }
            else throw std::invalid_argument("Unknown method: " + method);
            data = snapshot(service);
        }
        response["ok"] = true; response["data"] = std::move(data);
    } catch (const SonyException& ex) {
        response["error"] = {{"code", std::string(to_string(ex.code()))}, {"message", ex.what()}};
    } catch (const Json::exception& ex) {
        response["error"] = {{"code", "InvalidRequest"}, {"message", "Missing or incorrectly typed request field"}};
    } catch (const std::invalid_argument& ex) {
        response["error"] = {{"code", "InvalidRequest"}, {"message", ex.what()}};
    } catch (const std::exception& ex) {
        response["error"] = {{"code", "ServiceError"}, {"message", ex.what()}};
    }
    return response;
}
std::string JsonProtocol::executeLine(std::string_view line, IDeviceService& service) {
    try {
        auto boundedDepth = [](int depth, Json::parse_event_t, Json&) {
            if (depth > 32) throw std::invalid_argument("JSON nesting exceeds limit");
            return true;
        };
        return execute(Json::parse(line, boundedDepth), service).dump(-1, ' ', false, Json::error_handler_t::replace) + "\n"; }
    catch (const std::exception&) {
        return Json{{"version",Version},{"id",nullptr},{"ok",false},
            {"error",{{"code","InvalidRequest"},{"message","Malformed JSON request"}}}}.dump() + "\n";
    }
}
}
