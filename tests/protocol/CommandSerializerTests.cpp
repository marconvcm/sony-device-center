#include "CommandSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <initializer_list>

namespace
{
    Buffer bytes(std::initializer_list<unsigned int> values)
    {
        Buffer result;
        for (auto value : values)
            result.push_back(static_cast<char>(value));
        return result;
    }

    Buffer body(const Buffer& frame)
    {
        return Buffer(frame.begin() + 1, frame.end() - 1);
    }
}

TEST_CASE("MDR encoding matches a literal wire frame", "[serializer][framing]")
{
    // Legacy ambient inquiry: checksum = 0x0c + 1 + 2 + 0x66 + 2 = 0x77.
    const auto encoded = CommandSerializer::packageDataForBt(bytes({0x66, 0x02}), DATA_TYPE::DATA_MDR, 1);
    REQUIRE(encoded == bytes({0x3e, 0x0c, 0x01, 0, 0, 0, 0x02, 0x66, 0x02, 0x77, 0x3c}));
}

TEST_CASE("Empty ACK encoding includes the header and checksum", "[serializer][framing]")
{
    REQUIRE(CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, 0)
        == bytes({0x3e, 0x01, 0, 0, 0, 0, 0, 0x01, 0x3c}));
    REQUIRE(CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, 1)
        == bytes({0x3e, 0x01, 0x01, 0, 0, 0, 0, 0x02, 0x3c}));
}

TEST_CASE("Payload length is four-byte big-endian", "[serializer][framing]")
{
    REQUIRE(intToBytesBE(0x12345678) == std::vector<unsigned char>{0x12, 0x34, 0x56, 0x78});
    const Buffer payload(256, 0);
    const auto encoded = CommandSerializer::packageDataForBt(payload, DATA_TYPE::DATA_MDR, 0);
    REQUIRE(Buffer(encoded.begin(), encoded.begin() + 7) == bytes({0x3e, 0x0c, 0, 0, 0, 0x01, 0}));
    REQUIRE(Buffer(encoded.begin() + 7, encoded.end() - 2) == payload);
    REQUIRE(static_cast<unsigned char>(encoded[encoded.size() - 2]) == 0x0d);
    REQUIRE(CommandSerializer::unpackBtMessage(body(encoded)).payload == payload);
}

TEST_CASE("Sequence serialization retains the low byte", "[serializer][baseline]")
{
    // Characterizes serialization, not the session's intended one-bit ACK policy.
    const auto sequence = GENERATE(0u, 1u, 127u, 128u, 255u, 256u);
    const auto encoded = CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, sequence);
    const auto decoded = CommandSerializer::unpackBtMessage(body(encoded));
    REQUIRE(decoded.seqNumber == static_cast<unsigned char>(sequence));
    REQUIRE(decoded.dataType == DATA_TYPE::ACK);
    REQUIRE(decoded.payload.empty());
}

TEST_CASE("Reserved payload bytes have literal escape pairs", "[serializer][escaping]")
{
    const auto payload = bytes({0x3c, 0x3d, 0x3e});
    REQUIRE(CommandSerializer::_escapeSpecials(payload) == bytes({0x3d, 0x2c, 0x3d, 0x2d, 0x3d, 0x2e}));
    const auto encoded = CommandSerializer::packageDataForBt(payload, DATA_TYPE::DATA_MDR, 0);
    REQUIRE(encoded == bytes({0x3e, 0x0c, 0, 0, 0, 0, 3, 0x3d, 0x2c, 0x3d, 0x2d, 0x3d, 0x2e, 0xc6, 0x3c}));
    REQUIRE(CommandSerializer::unpackBtMessage(body(encoded)).payload == payload);
}

TEST_CASE("All byte values survive escape and frame round trips", "[serializer][escaping]")
{
    Buffer payload;
    for (unsigned int value = 0; value < 256; ++value)
        payload.push_back(static_cast<char>(value));
    REQUIRE(CommandSerializer::_unescapeSpecials(CommandSerializer::_escapeSpecials(payload)) == payload);
    const auto encoded = CommandSerializer::packageDataForBt(payload, DATA_TYPE::DATA_MDR, 1);
    const auto decoded = CommandSerializer::unpackBtMessage(body(encoded));
    REQUIRE(decoded.payload == payload);
    REQUIRE(decoded.seqNumber == 1);
    REQUIRE(decoded.dataType == DATA_TYPE::DATA_MDR);
    REQUIRE(CommandSerializer::_unescapeSpecials({}).empty());
    REQUIRE(CommandSerializer::_escapeSpecials({}).empty());
}

TEST_CASE("Escaping includes sequence length and checksum bytes", "[serializer][escaping]")
{
    SECTION("Sequence")
    {
        const auto encoded = CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, 0x3c);
        REQUIRE(encoded == bytes({0x3e, 0x01, 0x3d, 0x2c, 0, 0, 0, 0, 0x3d, 0x2d, 0x3c}));
    }
    SECTION("Length")
    {
        const auto size = GENERATE(0x3c, 0x3d, 0x3e);
        const Buffer payload(size, 0);
        const auto encoded = CommandSerializer::packageDataForBt(payload, DATA_TYPE::DATA_MDR, 0);
        REQUIRE(encoded[6] == 0x3d);
        REQUIRE(encoded[7] == size - 0x10);
        REQUIRE(CommandSerializer::unpackBtMessage(body(encoded)).payload == payload);
    }
    SECTION("Checksum")
    {
        const auto value = GENERATE(0x2f, 0x30, 0x31);
        const auto encoded = CommandSerializer::packageDataForBt(bytes({static_cast<unsigned int>(value)}), DATA_TYPE::DATA_MDR, 0);
        REQUIRE(encoded[encoded.size() - 3] == 0x3d);
        REQUIRE(encoded[encoded.size() - 2] == value - 3);
        REQUIRE(CommandSerializer::unpackBtMessage(body(encoded)).payload == bytes({static_cast<unsigned int>(value)}));
    }
}

TEST_CASE("Malformed escape sequences are rejected", "[serializer][escaping]")
{
    REQUIRE_THROWS_AS(CommandSerializer::_unescapeSpecials(bytes({0x3d})), std::runtime_error);
    REQUIRE_THROWS_AS(CommandSerializer::_unescapeSpecials(bytes({0x11, 0x3d})), std::runtime_error);
    REQUIRE_THROWS_AS(CommandSerializer::_unescapeSpecials(bytes({0x3d, 0x00})), std::runtime_error);
    REQUIRE_THROWS_AS(CommandSerializer::_unescapeSpecials(bytes({0x3d, 0x3e})), std::runtime_error);
}

TEST_CASE("Checksum is an unsigned modulo-256 sum", "[serializer][checksum]")
{
    const auto data = bytes({0xff, 0x80, 0x01});
    REQUIRE(CommandSerializer::_sumChecksum(data) == 0x80);
    REQUIRE(CommandSerializer::_sumChecksum(data.data(), 2) == 0x7f);
    REQUIRE(CommandSerializer::_sumChecksum({}) == 0);
}

TEST_CASE("Literal valid body decodes without delimiters", "[serializer][parsing]")
{
    const auto decoded = CommandSerializer::unpackBtMessage(bytes({0x0c, 1, 0, 0, 0, 2, 0x67, 2, 0x78}));
    REQUIRE(decoded.dataType == DATA_TYPE::DATA_MDR);
    REQUIRE(decoded.seqNumber == 1);
    REQUIRE(decoded.payload == bytes({0x67, 2}));
}

TEST_CASE("Invalid checksum and corrupted payload are rejected", "[serializer][checksum]")
{
    auto message = bytes({0x0c, 1, 0, 0, 0, 2, 0x67, 2, 0x78});
    SECTION("Checksum corruption") { message.back() ^= 1; }
    SECTION("Payload corruption") { message[6] ^= 1; }
    REQUIRE_THROWS_AS(CommandSerializer::unpackBtMessage(message), RecoverableException);
    try
    {
        CommandSerializer::unpackBtMessage(message);
    }
    catch (const RecoverableException& error)
    {
        REQUIRE(error.shouldDisconnect);
        REQUIRE(std::string(error.what()) == "Invalid checksum!");
    }
}

TEST_CASE("Every truncated prefix of a valid escaped body is rejected", "[serializer][parsing]")
{
    const auto complete = bytes({0x0c, 0, 0, 0, 0, 3, 0x3d, 0x2c, 0x3d, 0x2d, 0x3d, 0x2e, 0xc6});
    for (size_t length = 0; length < complete.size(); ++length)
    {
        DYNAMIC_SECTION("Body prefix length " << length)
        {
            REQUIRE_THROWS_AS(CommandSerializer::unpackBtMessage(Buffer(complete.begin(), complete.begin() + length)), std::runtime_error);
        }
    }
}

TEST_CASE("Declared lengths exceeding the received data are rejected", "[serializer][parsing]")
{
    auto message = bytes({0x0c, 0, 0, 0, 0, 2, 0x67, 2, 0x77});
    SECTION("One byte too many") { message[5] = 3; }
    SECTION("High big-endian byte") { message[2] = 1; }
    SECTION("Maximum 32-bit length")
    {
        for (size_t index = 2; index < 6; ++index)
            message[index] = static_cast<char>(0xff);
    }
    REQUIRE_THROWS_AS(CommandSerializer::unpackBtMessage(message), RecoverableException);
}

TEST_CASE("A shortened declaration with no matching checksum is rejected", "[serializer][parsing]")
{
    REQUIRE_THROWS_AS(CommandSerializer::unpackBtMessage(bytes({0x0c, 0, 0, 0, 0, 1, 0x67, 2, 0x77})), RecoverableException);
}

TEST_CASE("Baseline body decoder ignores bytes after a valid declared checksum", "[serializer][baseline]")
{
    // This permissive behavior is not a strict FrameCodec contract; see architecture-current.md.
    const auto decoded = CommandSerializer::unpackBtMessage(bytes({0x0c, 0, 0, 0, 0, 1, 0x67, 0x74, 0x55, 0x66}));
    REQUIRE(decoded.payload == bytes({0x67}));
}

TEST_CASE("Wire frames are not the body decoder input contract", "[serializer][parsing]")
{
    REQUIRE_THROWS_AS(CommandSerializer::unpackBtMessage(bytes({0x3e, 1, 0, 0, 0, 0, 0, 1, 0x3c})), std::runtime_error);
}

TEST_CASE("The encoded frame size limit includes escaping and delimiters", "[serializer][framing]")
{
    SECTION("Exactly at the limit")
    {
        REQUIRE(CommandSerializer::packageDataForBt(Buffer(2039, 0), DATA_TYPE::DATA_MDR, 0).size() == 2048);
    }
    SECTION("One byte over the limit")
    {
        REQUIRE_THROWS_AS(CommandSerializer::packageDataForBt(Buffer(2040, 0), DATA_TYPE::DATA_MDR, 0), std::runtime_error);
    }
    SECTION("Escape expansion exceeds the limit")
    {
        REQUIRE_THROWS_AS(CommandSerializer::packageDataForBt(Buffer(1020, 0x3c), DATA_TYPE::DATA_MDR, 0), std::runtime_error);
    }
}

TEST_CASE("Legacy NC and ambient command layouts remain unchanged", "[serializer][v1]")
{
    const auto level = GENERATE(0, 1, 2, 19);
    const unsigned int dualSingle = level == 0 ? 2 : (level == 1 ? 1 : 0);
    REQUIRE(CommandSerializer::serializeNcAndAsmSetting(
        NC_ASM_EFFECT::ADJUSTMENT_COMPLETION, NC_ASM_SETTING_TYPE::LEVEL_ADJUSTMENT,
        ASM_SETTING_TYPE::LEVEL_ADJUSTMENT, ASM_ID::VOICE, static_cast<char>(level))
        == bytes({0x68, 0x02, 0x11, 0x01, dualSingle, 0x01, 0x01, static_cast<unsigned int>(level)}));
    REQUIRE_THROWS_AS(CommandSerializer::getDualSingleForAsmLevel(20), std::runtime_error);
}

TEST_CASE("V2 NC and ambient use their distinct command layout", "[serializer][v2]")
{
    REQUIRE(CommandSerializer::serializeNcAndAsmSettingV2(
        NC_ASM_EFFECT::ON, NC_ASM_SETTING_TYPE_V2::NOISE_CANCELLING, ASM_ID::NORMAL, 1)
        == bytes({0x68, 0x17, 0x01, 0x01, 0, 0, 1}));
    REQUIRE(CommandSerializer::serializeNcAndAsmSettingV2(
        NC_ASM_EFFECT::ON, NC_ASM_SETTING_TYPE_V2::AMBIENT_SOUND, ASM_ID::VOICE, 20)
        == bytes({0x68, 0x17, 0x01, 0x01, 1, 1, 20}));
    REQUIRE(CommandSerializer::serializeNcAndAsmSettingV2(
        NC_ASM_EFFECT::OFF, NC_ASM_SETTING_TYPE_V2::NOISE_CANCELLING, ASM_ID::NORMAL, 1)
        == bytes({0x68, 0x17, 0x01, 0, 0, 0, 1}));
}

TEST_CASE("Legacy VPT and positioning serialization remains unchanged", "[serializer][v1]")
{
    REQUIRE(CommandSerializer::serializeVPTSetting(VPT_INQUIRED_TYPE::VPT, 3) == bytes({0x48, 1, 3}));
    REQUIRE(CommandSerializer::serializeVPTSetting(VPT_INQUIRED_TYPE::SOUND_POSITION, 0x12) == bytes({0x48, 2, 0x12}));
}
