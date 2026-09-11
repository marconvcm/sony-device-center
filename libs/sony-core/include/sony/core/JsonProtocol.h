#pragma once
#include "IDeviceService.h"
#include <nlohmann/json.hpp>
namespace sony::core {
class JsonProtocol {
public:
    using Json = nlohmann::json;
    static constexpr int Version = 1;
    static Json snapshot(IDeviceService& service);
    static Json execute(const Json& request, IDeviceService& service);
    static std::string executeLine(std::string_view line, IDeviceService& service);
};
}
