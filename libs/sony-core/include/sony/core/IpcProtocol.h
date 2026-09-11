#pragma once

#include "IDeviceService.h"
#include <string>
#include <string_view>
#include <vector>

namespace sony::core {

enum class IpcCommandType {
    Devices,
    Info,
    Battery,
    Anc,
    Ambient,
    EqGet,
    EqPreset,
    EqCustom,
    Dsee,
    AutoPowerOff,
    Status,
    Unknown
};

struct IpcCommand {
    IpcCommandType type{IpcCommandType::Unknown};
    std::vector<std::string> args;
    std::string raw;
};

struct IpcResponse {
    bool success{true};
    std::string message;
    std::string data;
};

class IpcProtocol {
public:
    static std::string executeLine(std::string_view line, IDeviceService& service);
    static IpcCommand parseCommand(std::string_view line);
    static std::string serializeResponse(const IpcResponse& response);
    static IpcResponse parseResponse(std::string_view line);
    static IpcResponse execute(const IpcCommand& cmd, IDeviceService& service);
};

} // namespace sony::core
