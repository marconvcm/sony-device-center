#include "sony/core/ControlSettings.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace sony::core {

std::string controlSettingsPath() {
#if defined(__linux__)
    std::filesystem::path config;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        config = xdg;
    if (!config.is_absolute()) {
        const char* userHome = std::getenv("HOME");
        if (!userHome || !*userHome) return {};
        config = std::filesystem::path(userHome) / ".config";
    }
    return (config / "sony-device-center" / "control.json").string();
#else
    return {};
#endif
}

bool bleControlEnabled(const std::string& path) {
    std::ifstream input(path);
    if (!input) return false;
    const auto settings = nlohmann::json::parse(input, nullptr, false);
    if (!settings.is_object()) return false;
    const auto preference = settings.find("bleControlEnabled");
    return preference != settings.end() && preference->is_boolean() && preference->get<bool>();
}

} // namespace sony::core
