#include "chanmap.h"
#include <algorithm>
#include "AudioFileX.h"

namespace chanmap {
    uint32_t getChannelMask(const std::vector<uint16_t>& channels)
    {
        uint32_t result = 0;
        for (size_t i = 0; i < channels.size(); ++i) {
            if (channels[i] >= 33)
                throw std::runtime_error("Not supported channel layout");
            result |= (1 << (channels[i] - 1));
        }
        return result;
    }
    void getChannels(uint32_t bitmap, std::vector<uint16_t> *result)
    {
        std::vector<uint16_t> channels;
        for (unsigned i = 0; i < 32; ++i) {
            if (bitmap & (1<<i))
                channels.push_back(i + 1);
        }
        result->swap(channels);
    }
    void getChannels(const AudioChannelLayout *acl, std::vector<uint16_t> *res)
    {
        std::vector<uint16_t> channels;
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
    void convertFromAppleLayout(const std::vector<uint16_t> &from,
                                std::vector<uint16_t> *to)
    {
        struct Simple {
            static uint16_t trans(uint16_t x) {
                switch (x) {
                case kAudioChannelLabel_Mono:
                    return kAudioChannelLabel_Center;
                /* XXX
                 * In case of SMPTE_DTV, Lt/Rt are used with L/R and others
                 * at the same time.
                 * Therefore Lt/Rt cannot be simply mapped into L/R.
                 */
                /*
                case kAudioChannelLabel_LeftTotal:
                    return kAudioChannelLabel_Left;
                case kAudioChannelLabel_RightTotal:
                    return kAudioChannelLabel_Right;
                */
                case kAudioChannelLabel_HeadphonesLeft:
                    return kAudioChannelLabel_Left;
                case kAudioChannelLabel_HeadphonesRight:
                    return kAudioChannelLabel_Right;
                }
                return x;
            }
        };
        struct RearSurround {
            static bool exists(uint16_t x)
            {
                return x == 33 || x == 34; /* Rls or Rrs */
            }
            static uint16_t trans(uint16_t x) {
                switch (x) {
                case 5  /* Ls  */: return 10 /* Lsd */;
                case 6  /* Rs  */: return 11 /* Rsd */;
                case 33 /* Rls */: return 5  /* Ls  */;
                case 34 /* Rrs */: return 6  /* Rs  */;
                }
                return x;
            }
        };
        std::vector<uint16_t> v(from.size());
        std::transform(from.begin(), from.end(), v.begin(), Simple::trans);
        if (std::find_if(v.begin(), v.end(), RearSurround::exists) != v.end())
            std::transform(v.begin(), v.end(), v.begin(), RearSurround::trans);
        to->swap(v);
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
    void getMappingToUSBOrder(const std::vector<uint16_t> &channels,
                              std::vector<uint32_t> *result)
    {
        std::vector<uint32_t> index(channels.size());
        for (unsigned i = 0; i < channels.size(); ++i)
            index[i] = i + 1;
        std::sort(index.begin(), index.end(),
                  IndexComparator<uint16_t>(&channels[0]));
        result->swap(index);
    }
}

