#ifndef HELPERS_H
#define HELPERS_H

#include <cstdint>
#include <sstream>

#define FOURCC(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))

namespace Helpers {
    inline uint32_t bitcount(uint32_t bits)
    {
        bits = (bits & 0x55555555) + (bits >> 1 & 0x55555555);
        bits = (bits & 0x33333333) + (bits >> 2 & 0x33333333);
        bits = (bits & 0x0f0f0f0f) + (bits >> 4 & 0x0f0f0f0f);
        bits = (bits & 0x00ff00ff) + (bits >> 8 & 0x00ff00ff);
        return (bits & 0x0000ffff) + (bits >>16 & 0x0000ffff);
    }
    template <typename ForwardIterator>
    inline bool is_increasing(ForwardIterator begin, ForwardIterator end)
    {
        if (begin == end)
            return true;
        for (ForwardIterator it; it = begin, ++begin != end; )
            if (*it >= *begin)
                return false;
        return true;
    }
    inline const char *channel_name(unsigned n)
    {
        const char *tab[] = {
            "?","FL","FR","FC","LF","BL","BR","FLC","FRC","BC",
            "SL","SR","TC","TFL","TFC","TFR","TBL","TBC","TBR"
        };
        return n <= 18 ? tab[n] : "?";
    }
    inline std::string describe_channels(uint32_t mask)
    {
        std::stringstream ss;
        ss << bitcount(mask) << ":";
        for (unsigned i = 0; i < 32; ++i)
            if (mask & (1<<i))
                ss << ' ' << channel_name(i + 1);
        return ss.str();
    }
}

#endif
