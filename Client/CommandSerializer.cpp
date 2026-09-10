#include "CommandSerializer.h"
#include "sony/protocol/FrameCodec.h"

using namespace sony;
using namespace sony::protocol;

constexpr int MAX_STEPS_WH_1000_XM3 = 19;

namespace CommandSerializer
{
	Buffer _escapeSpecials(const Buffer& src)
	{
		auto inSpan = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(src.data()), src.size());
		auto escaped = FrameCodec::escape(inSpan);
		return Buffer(escaped.begin(), escaped.end());
	}

	Buffer _unescapeSpecials(const Buffer& src)
	{
		auto inSpan = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(src.data()), src.size());
		auto unescaped = FrameCodec::unescape(inSpan);
		return Buffer(unescaped.begin(), unescaped.end());
	}

	unsigned char _sumChecksum(const char* src, size_t size)
	{
		auto inSpan = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(src), size);
		return FrameCodec::calculateChecksum(inSpan);
	}

	unsigned char _sumChecksum(const Buffer& src)
	{
		return _sumChecksum(src.data(), src.size());
	}

	Buffer packageDataForBt(const Buffer& src, DATA_TYPE dataType, unsigned int seqNumber)
	{
		SonyFrame frame{
			.type = static_cast<DataType>(dataType),
			.sequence = static_cast<uint8_t>(seqNumber),
			.payload = std::vector<uint8_t>(src.begin(), src.end())
		};
		auto encoded = FrameCodec::encode(frame);
		return Buffer(encoded.begin(), encoded.end());
	}

	Message unpackBtMessage(const Buffer& src)
	{
		try
		{
			auto inSpan = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(src.data()), src.size());
			auto frame = FrameCodec::decodeBody(inSpan);
			Message ret;
			ret.dataType = static_cast<DATA_TYPE>(frame.type);
			ret.seqNumber = frame.sequence;
			ret.payload = Buffer(frame.payload.begin(), frame.payload.end());
			return ret;
		}
		catch (const SonyException& e)
		{
			if (e.code() == SonyErrorCode::InvalidChecksum)
			{
				throw RecoverableException("Invalid checksum!", true);
			}
			if (e.code() == SonyErrorCode::InvalidFrame &&
			    std::string_view(e.what()).find("declared size") != std::string_view::npos)
			{
				throw RecoverableException("Invalid message: declared size exceeds received data", true);
			}
			throw;
		}
	}

	NC_DUAL_SINGLE_VALUE getDualSingleForAsmLevel(char asmLevel)
	{
		NC_DUAL_SINGLE_VALUE val = NC_DUAL_SINGLE_VALUE::OFF;
		if (asmLevel > MAX_STEPS_WH_1000_XM3)
		{
			throw std::runtime_error("Exceeded max steps");
		}
		else if (asmLevel == 1)
		{
			val = NC_DUAL_SINGLE_VALUE::SINGLE;
		}
		else if (asmLevel == 0)
		{
			val = NC_DUAL_SINGLE_VALUE::DUAL;
		}
		return val;
	}

	Buffer serializeNcAndAsmSetting(NC_ASM_EFFECT ncAsmEffect, NC_ASM_SETTING_TYPE ncAsmSettingType, ASM_SETTING_TYPE asmSettingType, ASM_ID asmId, char asmLevel)
	{
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(COMMAND_TYPE::NCASM_SET_PARAM));
		ret.push_back(static_cast<unsigned char>(NC_ASM_INQUIRED_TYPE::NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE));
		ret.push_back(static_cast<unsigned char>(ncAsmEffect));
		ret.push_back(static_cast<unsigned char>(ncAsmSettingType));
		ret.push_back(static_cast<unsigned char>(getDualSingleForAsmLevel(asmLevel)));
		ret.push_back(static_cast<unsigned char>(asmSettingType));
		ret.push_back(static_cast<unsigned char>(asmId));
		ret.push_back(asmLevel);
		return ret;
	}

	Buffer serializeNcAndAsmSettingV2(NC_ASM_EFFECT ncAsmEffect, NC_ASM_SETTING_TYPE_V2 ncAsmSettingType, ASM_ID voicePassthrough, char asmLevel)
	{
		// v2 layout: 0x68 | 0x17 | 0x01 (not dragging) | [NC/ASM on?] | [NC:0 ASM:1] | [voice passthrough?] | [level]
		// Reverse-engineered from https://github.com/mos9527/SonyHeadphonesClient (WF-1000XM5); WH family unverified.
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(COMMAND_TYPE::NCASM_SET_PARAM));
		ret.push_back(0x17);
		ret.push_back(0x01);
		ret.push_back(static_cast<unsigned char>(ncAsmEffect));
		ret.push_back(static_cast<unsigned char>(ncAsmSettingType));
		ret.push_back(static_cast<unsigned char>(voicePassthrough));
		ret.push_back(asmLevel);
		return ret;
	}

	Buffer serializeVPTSetting(VPT_INQUIRED_TYPE type, unsigned char preset)
	{
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(COMMAND_TYPE::VPT_SET_PARAM));
		ret.push_back(static_cast<unsigned char>(type));
		ret.push_back(preset);

		return ret;
	}

}

