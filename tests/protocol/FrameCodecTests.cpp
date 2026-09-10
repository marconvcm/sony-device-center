#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/SonyError.h"

using namespace sony::protocol;
using namespace sony;

namespace {

template <typename F>
void requireErrorCode(F&& fn, SonyErrorCode expectedCode) {
    try {
        fn();
        FAIL("Expected SonyException not thrown");
    } catch (const SonyException& e) {
        REQUIRE(e.code() == expectedCode);
    }
}

} // namespace

TEST_CASE("FrameCodec encodes a complete SonyFrame", "[protocol][codec]")
{
    // Legacy ambient inquiry: payload = {0x66, 0x02}, type = DataMdr (0x0c), seq = 1.
    // Checksum = 0x0c + 1 + 2 + 0x66 + 2 = 0x77.
    SonyFrame frame{
        .type = DataType::DataMdr,
        .sequence = 1,
        .payload = {0x66, 0x02}
    };

    auto encoded = FrameCodec::encode(frame);
    std::vector<uint8_t> expected = {0x3e, 0x0c, 0x01, 0, 0, 0, 0x02, 0x66, 0x02, 0x77, 0x3c};
    REQUIRE(encoded == expected);
}

TEST_CASE("FrameCodec decodes a complete wire frame into a SonyFrame", "[protocol][codec]")
{
    std::vector<uint8_t> wireFrame = {0x3e, 0x0c, 0x01, 0, 0, 0, 0x02, 0x66, 0x02, 0x77, 0x3c};
    auto decoded = FrameCodec::decode(wireFrame);

    REQUIRE(decoded.type == DataType::DataMdr);
    REQUIRE(decoded.sequence == 1);
    REQUIRE(decoded.payload == std::vector<uint8_t>{0x66, 0x02});
}

TEST_CASE("FrameCodec encodes and decodes empty payload (ACK)", "[protocol][codec]")
{
    SonyFrame ackFrame{
        .type = DataType::Ack,
        .sequence = 0,
        .payload = {}
    };

    auto encoded = FrameCodec::encode(ackFrame);
    std::vector<uint8_t> expected = {0x3e, 0x01, 0, 0, 0, 0, 0, 0x01, 0x3c};
    REQUIRE(encoded == expected);

    auto decoded = FrameCodec::decode(encoded);
    REQUIRE(decoded == ackFrame);
}

TEST_CASE("FrameCodec round trips with reserved bytes 0x3c 0x3d 0x3e", "[protocol][codec]")
{
    SonyFrame frame{
        .type = DataType::DataMdr,
        .sequence = 0,
        .payload = {0x3c, 0x3d, 0x3e}
    };

    auto encoded = FrameCodec::encode(frame);
    auto decoded = FrameCodec::decode(encoded);
    REQUIRE(decoded == frame);
}

TEST_CASE("FrameCodec escape and unescape all byte values round trip", "[protocol][codec]")
{
    std::vector<uint8_t> allBytes;
    allBytes.reserve(256);
    for (int i = 0; i < 256; ++i) {
        allBytes.push_back(static_cast<uint8_t>(i));
    }

    auto escaped = FrameCodec::escape(allBytes);
    auto unescaped = FrameCodec::unescape(escaped);
    REQUIRE(unescaped == allBytes);
}

TEST_CASE("FrameCodec unescape rejects malformed escape sequences", "[protocol][codec]")
{
    requireErrorCode([]() {
        FrameCodec::unescape(std::vector<uint8_t>{0x3d});
    }, SonyErrorCode::InvalidFrame);

    requireErrorCode([]() {
        FrameCodec::unescape(std::vector<uint8_t>{0x11, 0x3d});
    }, SonyErrorCode::InvalidFrame);

    requireErrorCode([]() {
        FrameCodec::unescape(std::vector<uint8_t>{0x3d, 0x00});
    }, SonyErrorCode::InvalidFrame);

    requireErrorCode([]() {
        FrameCodec::unescape(std::vector<uint8_t>{0x3d, 0x3e});
    }, SonyErrorCode::InvalidFrame);
}

TEST_CASE("FrameCodec calculateChecksum returns unsigned modulo-256 sum", "[protocol][codec]")
{
    std::vector<uint8_t> data = {0xff, 0x80, 0x01};
    REQUIRE(FrameCodec::calculateChecksum(data) == 0x80);
    REQUIRE(FrameCodec::calculateChecksum(std::span(data.data(), 2)) == 0x7f);
    REQUIRE(FrameCodec::calculateChecksum({}) == 0);
}

TEST_CASE("FrameCodec decode rejects corrupted checksum", "[protocol][codec]")
{
    std::vector<uint8_t> wireFrame = {0x3e, 0x0c, 0x01, 0, 0, 0, 0x02, 0x66, 0x02, 0x77, 0x3c};
    wireFrame[wireFrame.size() - 2] ^= 0x01; // corrupt checksum

    requireErrorCode([&]() {
        FrameCodec::decode(wireFrame);
    }, SonyErrorCode::InvalidChecksum);
}

TEST_CASE("FrameCodec decode rejects corrupted payload", "[protocol][codec]")
{
    std::vector<uint8_t> wireFrame = {0x3e, 0x0c, 0x01, 0, 0, 0, 0x02, 0x66, 0x02, 0x77, 0x3c};
    wireFrame[7] ^= 0x01; // corrupt payload byte

    requireErrorCode([&]() {
        FrameCodec::decode(wireFrame);
    }, SonyErrorCode::InvalidChecksum);
}

TEST_CASE("FrameCodec decode rejects missing or invalid delimiters", "[protocol][codec]")
{
    std::vector<uint8_t> valid = {0x3e, 0x0c, 0x01, 0, 0, 0, 0x02, 0x66, 0x02, 0x77, 0x3c};

    // Missing START delimiter
    auto noStart = valid;
    noStart.front() = 0x00;
    requireErrorCode([&]() { FrameCodec::decode(noStart); }, SonyErrorCode::InvalidFrame);

    // Missing END delimiter
    auto noEnd = valid;
    noEnd.back() = 0x00;
    requireErrorCode([&]() { FrameCodec::decode(noEnd); }, SonyErrorCode::InvalidFrame);

    // Too small for delimiters
    requireErrorCode([]() { FrameCodec::decode(std::vector<uint8_t>{0x3e}); }, SonyErrorCode::InvalidFrame);
}

TEST_CASE("FrameCodec decodeBody decodes un-delimited body", "[protocol][codec]")
{
    // Body without delimiters 0x3e and 0x3c
    std::vector<uint8_t> body = {0x0c, 0x01, 0, 0, 0, 0x02, 0x66, 0x02, 0x77};
    auto decoded = FrameCodec::decodeBody(body);

    REQUIRE(decoded.type == DataType::DataMdr);
    REQUIRE(decoded.sequence == 1);
    REQUIRE(decoded.payload == std::vector<uint8_t>{0x66, 0x02});
}

TEST_CASE("FrameCodec decode rejects declared length exceeding body data", "[protocol][codec]")
{
    std::vector<uint8_t> body = {0x0c, 0, 0, 0, 0, 2, 0x67, 2, 0x77};
    body[5] = 10; // declared length 10 but only 2 payload bytes provided

    requireErrorCode([&]() {
        FrameCodec::decodeBody(body);
    }, SonyErrorCode::InvalidFrame);
}

TEST_CASE("FrameCodec encode rejects frames exceeding MAX_FRAME_SIZE", "[protocol][codec]")
{
    SonyFrame frame{
        .type = DataType::DataMdr,
        .sequence = 0,
        .payload = std::vector<uint8_t>(2040, 0)
    };

    requireErrorCode([&]() {
        FrameCodec::encode(frame);
    }, SonyErrorCode::InvalidFrame);
}
