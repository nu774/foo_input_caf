#include "chanmap.h"
#include <algorithm>

namespace chanmap {
    uint32_t getChannelMask(const std::vector<char>& channels)
    {
	uint32_t result = 0;
	for (size_t i = 0; i < channels.size(); ++i) {
	    if (channels[i] >= 33)
		throw std::runtime_error("Not supported channel layout");
	    result |= (1 << (channels[i] - 1));
	}
	return result;
    }
    void getChannels(uint32_t bitmap, std::vector<char> *result)
    {
	std::vector<char> channels;
	for (unsigned i = 0; i < 32; ++i) {
	    if (bitmap & (1<<i))
		channels.push_back(i + 1);
	}
	result->swap(channels);
    }
    void getChannels(const AudioChannelLayout *acl, std::vector<char> *res)
    {
	std::vector<char> channels;
	uint32_t bitmap = 0;
	const char *layout = 0;

	switch (acl->mChannelLayoutTag) {
	case kAudioChannelLayoutTag_UseChannelBitmap:
	    bitmap = acl->mChannelBitmap; break;
	case kAudioChannelLayoutTag_UseChannelDescriptions:
	{
	    const AudioChannelDescription *desc = acl->mChannelDescriptions;
	    for (size_t i = 0; i < acl->mNumberChannelDescriptions; ++i)
		channels.push_back(desc[i].mChannelLabel);
	    break;
	}
	case kAudioChannelLayoutTag_Mono:
	    layout = "\x03"; break;
	case kAudioChannelLayoutTag_AC3_1_0_1:
	    layout = "\x03\x04"; break;
	case kAudioChannelLayoutTag_Stereo:
	    layout = "\x01\x02"; break;
	case kAudioChannelLayoutTag_DVD_4:
	    layout = "\x01\x02\x04"; break;
	case kAudioChannelLayoutTag_MPEG_3_0_A:
	    layout = "\x01\x02\x03"; break;
	case kAudioChannelLayoutTag_MPEG_3_0_B:
	    layout = "\x03\x01\x02"; break;
	case kAudioChannelLayoutTag_AC3_3_0:
	    layout = "\x01\x03\x02"; break;
	case kAudioChannelLayoutTag_ITU_2_1:
	    layout = "\x01\x02\x09"; break;
	case kAudioChannelLayoutTag_DVD_10:
	    layout = "\x01\x02\x03\x04"; break;
	case kAudioChannelLayoutTag_AC3_3_0_1:
	    layout = "\x01\x03\x02\x04"; break;
	case kAudioChannelLayoutTag_DVD_5:
	    layout = "\x01\x02\x04\x09"; break;
	case kAudioChannelLayoutTag_AC3_2_1_1:
	    layout = "\x01\x02\x09\x04"; break;
	case kAudioChannelLayoutTag_Quadraphonic:
	case kAudioChannelLayoutTag_ITU_2_2:
	    layout = "\x01\x02\x05\x06"; break;
	case kAudioChannelLayoutTag_MPEG_4_0_A:
	    layout = "\x01\x02\x03\x09"; break;
	case kAudioChannelLayoutTag_MPEG_4_0_B:
	    layout = "\x03\x01\x02\x09"; break;
	case kAudioChannelLayoutTag_AC3_3_1:
	    layout = "\x01\x03\x02\x09"; break;
	case kAudioChannelLayoutTag_DVD_6:
	    layout = "\x01\x02\x04\x05\x06"; break;
	case kAudioChannelLayoutTag_DVD_18:
	    layout = "\x01\x02\x05\x06\x04"; break;
	case kAudioChannelLayoutTag_DVD_11:
	    layout = "\x01\x02\x03\x04\x09"; break;
	case kAudioChannelLayoutTag_AC3_3_1_1:
	    layout = "\x01\x03\x02\x09\x04"; break;
	case kAudioChannelLayoutTag_MPEG_5_0_A:
	    layout = "\x01\x02\x03\x05\x06"; break;
	case kAudioChannelLayoutTag_MPEG_5_0_B:
	    layout = "\x01\x02\x05\x06\x03"; break;
	case kAudioChannelLayoutTag_MPEG_5_0_C:
	    layout = "\x01\x03\x02\x05\x06"; break;
	case kAudioChannelLayoutTag_MPEG_5_0_D:
	    layout = "\x03\x01\x02\x05\x06"; break;
	case kAudioChannelLayoutTag_MPEG_5_1_A:
	    layout = "\x01\x02\x03\x04\x05\x06"; break;
	case kAudioChannelLayoutTag_MPEG_5_1_B:
	    layout = "\x01\x02\x05\x06\x03\x04"; break;
	case kAudioChannelLayoutTag_MPEG_5_1_C:
	    layout = "\x01\x03\x02\x05\x06\x04"; break;
	case kAudioChannelLayoutTag_MPEG_5_1_D:
	    layout = "\x03\x01\x02\x05\x06\x04"; break;
	case kAudioChannelLayoutTag_Hexagonal:
	case kAudioChannelLayoutTag_AudioUnit_6_0:
	    layout = "\x01\x02\x05\x06\x03\x09"; break;
	case kAudioChannelLayoutTag_AAC_6_0:
	    layout = "\x03\x01\x02\x05\x06\x09"; break;
	case kAudioChannelLayoutTag_MPEG_6_1_A:
	    layout = "\x01\x02\x03\x04\x05\x06\x09"; break;
	case kAudioChannelLayoutTag_AAC_6_1:
	    layout = "\x03\x01\x02\x05\x06\x09\x04"; break;
	case kAudioChannelLayoutTag_AudioUnit_7_0:
	    layout = "\x01\x02\x05\x06\x03\x21\x22"; break;
	case kAudioChannelLayoutTag_AudioUnit_7_0_Front:
	    layout = "\x01\x02\x05\x06\x03\x07\x08"; break;
	case kAudioChannelLayoutTag_AAC_7_0:
	    layout = "\x03\x01\x02\x05\x06\x21\x22"; break;
	case kAudioChannelLayoutTag_MPEG_7_1_A:
	    layout = "\x01\x02\x03\x04\x05\x06\x07\x08"; break;
	case kAudioChannelLayoutTag_MPEG_7_1_B:
	    layout = "\x03\x07\x08\x01\x02\x05\x06\x04"; break;
	case kAudioChannelLayoutTag_MPEG_7_1_C:
	    layout = "\x01\x02\x03\x04\x05\x06\x21\x22"; break;
	case kAudioChannelLayoutTag_Emagic_Default_7_1:
	    layout = "\x01\x02\x05\x06\x03\x04\x07\x08"; break;
	case kAudioChannelLayoutTag_Octagonal:
	    layout = "\x01\x02\x05\x06\x03\x09\x21\x22"; break;
	case kAudioChannelLayoutTag_AAC_Octagonal:
	    layout = "\x03\x01\x02\x05\x06\x21\x22\x09"; break;
	default:
	    throw std::runtime_error("Unsupported channel layout");
	}

	if (bitmap)
	    getChannels(bitmap, &channels);
	else if (layout)
	    while (*layout) channels.push_back(*layout++);

	res->swap(channels);
    }
    void convertFromAppleLayout(const std::vector<char> &from,
				std::vector<char> *to)
    {
	struct F {
	    static bool test(char x) { return x == 33 || x == 34; }
	    static char trans(char x) {
		switch (x) {
		case 5: return 10;
		case 6: return 11;
		case 33: return 5;
		case 34: return 6;
		}
		return x;
	    }
	};
	bool has_rear =
	    std::find_if(from.begin(), from.end(), F::test) != from.end();
	std::vector<char> result(from.size());
	if (!has_rear)
	    std::copy(from.begin(), from.end(), result.begin());
	else
	    std::transform(from.begin(), from.end(), result.begin(), F::trans);
	to->swap(result);
    }

    template <typename T>
    class IndexComparator {
	const T *m_data;
    public:
	IndexComparator(const T *data): m_data(data) {}
	bool operator()(size_t l, size_t r) {
	    return m_data[l-1] < m_data[r-1];
	}
    };
    void getMappingToUSBOrder(const std::vector<char> &channels,
			      std::vector<uint32_t> *result)
    {
	std::vector<uint32_t> index(channels.size());
	for (unsigned i = 0; i < channels.size(); ++i)
	    index[i] = i + 1;
	std::sort(index.begin(), index.end(),
		  IndexComparator<char>(&channels[0]));
	result->swap(index);
    }
}

