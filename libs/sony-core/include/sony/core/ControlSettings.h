#pragma once

#include <string>

namespace sony::core {

// Shared by GUI, daemon and CLI; preferences apply at process startup.
std::string controlSettingsPath();
bool bleControlEnabled(const std::string& path = controlSettingsPath());

} // namespace sony::core
