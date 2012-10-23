#ifndef CHANMAP_H
#define CHANMAP_H

#include <vector>
#include <stdint.h>
#include "CoreAudio/CoreAudioTypes.h"

namespace chanmap {
    uint32_t getChannelMask(const std::vector<char>& channels);
    void getChannels(uint32_t bitmap, std::vector<char> *result);
    void getChannels(const AudioChannelLayout *acl, std::vector<char> *res);
    void convertFromAppleLayout(const std::vector<char> &from,
				std::vector<char> *to);
    void getMappingToUSBOrder(const std::vector<char> &channels,
			      std::vector<uint32_t> *result);
}

#endif

