#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace sony {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Off
};

[[nodiscard]] constexpr std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Off:   return "OFF";
    }
    return "UNKNOWN";
}

namespace LogCategory {
    inline constexpr std::string_view Transport    = "sony.transport";
    inline constexpr std::string_view Protocol     = "sony.protocol";
    inline constexpr std::string_view Session      = "sony.session";
    inline constexpr std::string_view Device       = "sony.device";
    inline constexpr std::string_view Capabilities = "sony.capabilities";
    inline constexpr std::string_view State        = "sony.state";
} // namespace LogCategory

class Logger {
public:
    using LogSink = std::function<void(LogLevel level, std::string_view category, std::string_view message)>;

    static void setLogLevel(LogLevel level) noexcept;
    [[nodiscard]] static LogLevel getLogLevel() noexcept;

    static void setDeveloperMode(bool enabled) noexcept;
    [[nodiscard]] static bool isDeveloperMode() noexcept;

    static void setLogSink(LogSink sink);
    static void resetLogSink();

    static void log(LogLevel level, std::string_view category, std::string_view message);
    static void debug(std::string_view category, std::string_view message);
    static void info(std::string_view category, std::string_view message);
    static void warn(std::string_view category, std::string_view message);
    static void error(std::string_view category, std::string_view message);

    // Diagnostics / developer logging
    static void logTx(std::span<const uint8_t> frameBytes, std::string_view semanticDesc = "");
    static void logRx(std::span<const uint8_t> frameBytes, std::string_view semanticDesc = "");

    static std::string formatHex(std::span<const uint8_t> bytes, size_t maxBytes = 32);
    static std::string describePayload(std::span<const uint8_t> payload);
};

} // namespace sony
