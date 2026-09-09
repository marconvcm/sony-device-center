#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace sony {

enum class SonyErrorCode {
    Timeout,
    Disconnected,
    Unsupported,
    InvalidFrame,
    InvalidChecksum,
    InvalidResponse,
    TransportFailure,
    ProtocolViolation
};

[[nodiscard]] constexpr std::string_view to_string(SonyErrorCode code) noexcept {
    switch (code) {
        case SonyErrorCode::Timeout: return "Timeout";
        case SonyErrorCode::Disconnected: return "Disconnected";
        case SonyErrorCode::Unsupported: return "Unsupported";
        case SonyErrorCode::InvalidFrame: return "InvalidFrame";
        case SonyErrorCode::InvalidChecksum: return "InvalidChecksum";
        case SonyErrorCode::InvalidResponse: return "InvalidResponse";
        case SonyErrorCode::TransportFailure: return "TransportFailure";
        case SonyErrorCode::ProtocolViolation: return "ProtocolViolation";
    }
    return "Unknown";
}

class SonyException : public std::runtime_error {
public:
    explicit SonyException(SonyErrorCode code, const std::string& message)
        : std::runtime_error(message), _code(code) {}

    explicit SonyException(SonyErrorCode code)
        : std::runtime_error(std::string(to_string(code))), _code(code) {}

    [[nodiscard]] SonyErrorCode code() const noexcept { return _code; }

private:
    SonyErrorCode _code;
};

} // namespace sony
