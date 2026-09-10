#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/SonyError.h"
#include <string>

using namespace sony::protocol;

TEST_CASE("SonyErrorCode to_string conversion", "[error]") {
    CHECK(to_string(SonyErrorCode::Timeout) == "Timeout");
    CHECK(to_string(SonyErrorCode::Disconnected) == "Disconnected");
    CHECK(to_string(SonyErrorCode::Unsupported) == "Unsupported");
    CHECK(to_string(SonyErrorCode::InvalidFrame) == "InvalidFrame");
    CHECK(to_string(SonyErrorCode::InvalidChecksum) == "InvalidChecksum");
    CHECK(to_string(SonyErrorCode::InvalidResponse) == "InvalidResponse");
    CHECK(to_string(SonyErrorCode::TransportFailure) == "TransportFailure");
    CHECK(to_string(SonyErrorCode::ProtocolViolation) == "ProtocolViolation");

    auto unknownCode = static_cast<SonyErrorCode>(999);
    CHECK(to_string(unknownCode) == "Unknown");
}

TEST_CASE("SonyException properties and polymorphism", "[error]") {
    SonyException exWithMsg(SonyErrorCode::InvalidChecksum, "CRC mismatch at byte 12");
    CHECK(exWithMsg.code() == SonyErrorCode::InvalidChecksum);
    CHECK(std::string(exWithMsg.what()) == "CRC mismatch at byte 12");

    SonyException exDefaultMsg(SonyErrorCode::Timeout);
    CHECK(exDefaultMsg.code() == SonyErrorCode::Timeout);
    CHECK(std::string(exDefaultMsg.what()) == "Timeout");

    // Catch by base reference
    try {
        throw SonyException(SonyErrorCode::ProtocolViolation, "Opcode 0x22 sent on V1");
    } catch (const std::runtime_error& err) {
        CHECK(std::string(err.what()) == "Opcode 0x22 sent on V1");
    }

    // Catch by typed reference
    try {
        throw SonyException(SonyErrorCode::Unsupported, "Feature not available on WH-CH720N");
    } catch (const SonyException& err) {
        CHECK(err.code() == SonyErrorCode::Unsupported);
        CHECK(std::string(err.what()) == "Feature not available on WH-CH720N");
    }
}
