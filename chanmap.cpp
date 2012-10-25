#include "chanmap.h"
#include <algorithm>
#include "AudioFileX.h"

namespace chanmap {
    uint32_t getChannelMask(const std::vector<char>& channels)
    {
	if (channels.size() == 1) {
	    // kAudioChannelLabel_Mono(42) might be used.
	    // As a channel mask, we always use center(3) for mono.
	    return 1 << (3 - 1);
	}
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
	std::shared_ptr<AudioChannelLayout> layout;

	switch (acl->mChannelLayoutTag) {
	case kAudioChannelLayoutTag_UseChannelBitmap:
	    getChannels(acl->mChannelBitmap, &channels);
	    res->swap(channels);
	    return;
	case kAudioChannelLayoutTag_UseChannelDescriptions:
	    break;
	default:
	    afutil::getChannelLayoutForTag(acl->mChannelLayoutTag, &layout);
	    acl = layout.get();
	    break;
	}
	const AudioChannelDescription *desc = acl->mChannelDescriptions;
	for (size_t i = 0; i < acl->mNumberChannelDescriptions; ++i)
	    channels.push_back(desc[i].mChannelLabel);
	res->swap(channels);
    }
    void convertFromAppleLayout(const std::vector<char> &from,
				std::vector<char> *to)
    {
	struct RearSurround {
	    static bool exists(char x) { return x == 33 || x == 34; }
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
	bool has_rear = std::find_if(from.begin(), from.end(),
				     RearSurround::exists) != from.end();
	std::vector<char> result(from.size());
	if (!has_rear)
	    std::copy(from.begin(), from.end(), result.begin());
	else
	    std::transform(from.begin(), from.end(), result.begin(),
			   RearSurround::trans);
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

