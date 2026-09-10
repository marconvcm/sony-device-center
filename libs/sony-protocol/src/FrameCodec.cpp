#include "sony/protocol/FrameCodec.h"
#include <numeric>

namespace sony::protocol {

std::vector<uint8_t> FrameCodec::escape(std::span<const uint8_t> data) {
    std::vector<uint8_t> result;
    result.reserve(data.size());

    for (const uint8_t b : data) {
        switch (b) {
            case END_MARKER: // 60 (0x3c)
                result.push_back(ESCAPE_MARKER); // 61 (0x3d)
                result.push_back(ESCAPED_60);    // 44 (0x2c)
                break;
            case ESCAPE_MARKER: // 61 (0x3d)
                result.push_back(ESCAPE_MARKER);
                result.push_back(ESCAPED_61);    // 45 (0x2d)
                break;
            case START_MARKER: // 62 (0x3e)
                result.push_back(ESCAPE_MARKER);
                result.push_back(ESCAPED_62);    // 46 (0x2e)
                break;
            default:
                result.push_back(b);
                break;
        }
    }

    return result;
}

std::vector<uint8_t> FrameCodec::unescape(std::span<const uint8_t> data) {
    std::vector<uint8_t> result;
    result.reserve(data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        const uint8_t curr = data[i];
        if (curr == ESCAPE_MARKER) {
            if (i + 1 >= data.size()) {
                throw SonyException(SonyErrorCode::InvalidFrame, "No data left for escaped byte data");
            }
            ++i;
            switch (data[i]) {
                case ESCAPED_60:
                    result.push_back(END_MARKER);
                    break;
                case ESCAPED_61:
                    result.push_back(ESCAPE_MARKER);
                    break;
                case ESCAPED_62:
                    result.push_back(START_MARKER);
                    break;
                default:
                    throw SonyException(SonyErrorCode::InvalidFrame, "Unexpected escaped byte");
            }
        } else {
            result.push_back(curr);
        }
    }

    return result;
}

uint8_t FrameCodec::calculateChecksum(std::span<const uint8_t> unescapedData) {
    uint8_t accumulator = 0;
    for (const uint8_t b : unescapedData) {
        accumulator += b;
    }
    return accumulator;
}

std::vector<uint8_t> FrameCodec::encode(const SonyFrame& frame) {
    std::vector<uint8_t> toEscape;
    toEscape.reserve(frame.payload.size() + 2 + 4 + 1);

    toEscape.push_back(static_cast<uint8_t>(frame.type));
    toEscape.push_back(frame.sequence);

    const size_t payloadSize = frame.payload.size();
    toEscape.push_back(static_cast<uint8_t>((payloadSize >> 24) & 0xff));
    toEscape.push_back(static_cast<uint8_t>((payloadSize >> 16) & 0xff));
    toEscape.push_back(static_cast<uint8_t>((payloadSize >> 8) & 0xff));
    toEscape.push_back(static_cast<uint8_t>(payloadSize & 0xff));

    toEscape.insert(toEscape.end(), frame.payload.begin(), frame.payload.end());

    const uint8_t checksum = calculateChecksum(toEscape);
    toEscape.push_back(checksum);

    auto escaped = escape(toEscape);

    if (escaped.size() + 2 > MAX_FRAME_SIZE) {
        throw SonyException(SonyErrorCode::InvalidFrame, "Exceeded the max bluetooth message size, and I can't handle chunked messages");
    }

    std::vector<uint8_t> result;
    result.reserve(escaped.size() + 2);
    result.push_back(START_MARKER);
    result.insert(result.end(), escaped.begin(), escaped.end());
    result.push_back(END_MARKER);

    return result;
}

SonyFrame FrameCodec::decode(std::span<const uint8_t> frameData) {
    if (frameData.size() < 2) {
        throw SonyException(SonyErrorCode::InvalidFrame, "Frame too small for delimiters");
    }

    if (frameData.front() != START_MARKER || frameData.back() != END_MARKER) {
        throw SonyException(SonyErrorCode::InvalidFrame, "Invalid frame delimiters");
    }

    auto body = frameData.subspan(1, frameData.size() - 2);
    return decodeBody(body);
}

SonyFrame FrameCodec::decodeBody(std::span<const uint8_t> bodyData) {
    auto unescaped = unescape(bodyData);

    if (unescaped.size() < MIN_BODY_SIZE) {
        throw SonyException(SonyErrorCode::InvalidFrame, "Invalid message: Smaller than the minimum message size");
    }

    const size_t dataSize = (static_cast<size_t>(unescaped[2]) << 24) |
                            (static_cast<size_t>(unescaped[3]) << 16) |
                            (static_cast<size_t>(unescaped[4]) << 8)  |
                            (static_cast<size_t>(unescaped[5]));

    if (unescaped.size() < 6 + dataSize + 1) {
        throw SonyException(SonyErrorCode::InvalidFrame, "Invalid message: declared size exceeds received data");
    }

    const uint8_t expectedChecksum = unescaped[6 + dataSize];
    const uint8_t actualChecksum = calculateChecksum(std::span<const uint8_t>(unescaped.data(), 6 + dataSize));

    if (expectedChecksum != actualChecksum) {
        throw SonyException(SonyErrorCode::InvalidChecksum, "Invalid checksum!");
    }

    SonyFrame frame;
    frame.type = static_cast<DataType>(unescaped[0]);
    frame.sequence = unescaped[1];
    frame.payload.assign(unescaped.begin() + 6, unescaped.begin() + 6 + dataSize);

    return frame;
}

} // namespace sony::protocol
