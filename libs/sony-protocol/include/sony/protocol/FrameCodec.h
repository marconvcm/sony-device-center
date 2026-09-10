#pragma once

#include "SonyFrame.h"
#include "sony/transport/SonyError.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sony::protocol {

class FrameCodec {
public:
    static constexpr uint8_t START_MARKER  = 0x3e; // 62
    static constexpr uint8_t END_MARKER    = 0x3c; // 60
    static constexpr uint8_t ESCAPE_MARKER = 0x3d; // 61
    static constexpr uint8_t ESCAPED_60    = 0x2c; // 44
    static constexpr uint8_t ESCAPED_61    = 0x2d; // 45
    static constexpr uint8_t ESCAPED_62    = 0x2e; // 46

    static constexpr size_t MAX_FRAME_SIZE = 2048;
    static constexpr size_t MIN_BODY_SIZE  = 7; // type(1) + seq(1) + len(4) + checksum(1)

    // Encode a SonyFrame into a complete wire frame (<START_MARKER> ... <END_MARKER>)
    static std::vector<uint8_t> encode(const SonyFrame& frame);

    // Decode a complete wire frame (<START_MARKER> ... <END_MARKER>) into a SonyFrame
    static SonyFrame decode(std::span<const uint8_t> frameData);

    // Decode un-delimited body bytes (delimiters already stripped) into a SonyFrame
    static SonyFrame decodeBody(std::span<const uint8_t> bodyData);

    // Byte escaping utilities
    static std::vector<uint8_t> escape(std::span<const uint8_t> data);
    static std::vector<uint8_t> unescape(std::span<const uint8_t> data);

    // Checksum calculation (unsigned modulo-256 sum over unescaped bytes)
    static uint8_t calculateChecksum(std::span<const uint8_t> unescapedData);
};

} // namespace sony::protocol
