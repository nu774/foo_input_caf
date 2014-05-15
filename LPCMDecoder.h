#ifndef LPCMDECODER_H
#define LPCMDECODER_H

#include "Decoder.h"

class LPCMDecoder: public DecoderBase {
    CAFFile::Format m_format;
    bool            m_need_channel_remap;
public:
    LPCMDecoder(const CAFFile::Format &format)
        : m_format(format), m_need_channel_remap(false)
    {
        auto chanmap = m_format.channel_map;
        if (chanmap.size()
         && !Helpers::is_increasing(chanmap.begin(), chanmap.end()))
            m_need_channel_remap = true;
    }
    void get_info(file_info &info);
    void decode(const void *buffer, t_size bytes, audio_chunk &chunk,
                abort_callback &abort);
};

#endif
