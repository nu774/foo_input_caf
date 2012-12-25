#ifndef CHANMAP_H
#define CHANMAP_H

#include <vector>
#include <stdint.h>
#include "CoreAudio/CoreAudioTypes.h"

namespace chanmap {
    uint32_t getChannelMask(const std::vector<uint16_t>& channels);
    void getChannels(uint32_t bitmap, std::vector<uint16_t> *result);
    void getChannels(const AudioChannelLayout *acl, std::vector<uint16_t> *res);
    void convertFromAppleLayout(const std::vector<uint16_t> &from,
                                std::vector<uint16_t> *to);
    void getMappingToUSBOrder(const std::vector<uint16_t> &channels,
                              std::vector<uint32_t> *result);
}

#endif

