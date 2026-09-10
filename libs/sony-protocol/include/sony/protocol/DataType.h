#pragma once

#include <cstdint>
#include <string_view>

namespace sony::protocol {

enum class DataType : uint8_t {
    Data = 0,
    Ack = 1,
    DataMcNo1 = 2,
    DataIcd = 9,
    DataEv = 10,
    DataMdr = 12,
    DataCommon = 13,
    DataMdrNo2 = 14,
    Shot = 16,
    ShotMcNo1 = 18,
    ShotIcd = 25,
    ShotEv = 26,
    ShotMdr = 28,
    ShotCommon = 29,
    ShotMdrNo2 = 30,
    LargeDataCommon = 45,
    Unknown = 255
};

[[nodiscard]] constexpr std::string_view to_string(DataType type) noexcept {
    switch (type) {
        case DataType::Data: return "Data";
        case DataType::Ack: return "Ack";
        case DataType::DataMcNo1: return "DataMcNo1";
        case DataType::DataIcd: return "DataIcd";
        case DataType::DataEv: return "DataEv";
        case DataType::DataMdr: return "DataMdr";
        case DataType::DataCommon: return "DataCommon";
        case DataType::DataMdrNo2: return "DataMdrNo2";
        case DataType::Shot: return "Shot";
        case DataType::ShotMcNo1: return "ShotMcNo1";
        case DataType::ShotIcd: return "ShotIcd";
        case DataType::ShotEv: return "ShotEv";
        case DataType::ShotMdr: return "ShotMdr";
        case DataType::ShotCommon: return "ShotCommon";
        case DataType::ShotMdrNo2: return "ShotMdrNo2";
        case DataType::LargeDataCommon: return "LargeDataCommon";
        case DataType::Unknown: return "Unknown";
    }
    return "Unknown";
}

} // namespace sony::protocol
