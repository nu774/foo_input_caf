#ifndef PACKETDECODER_H
#define PACKETDECODER_H

#include "Decoder.h"

class PacketDecoder: public IDecoder {
    service_ptr_t<packet_decoder> m_decoder;
public:
    PacketDecoder(const GUID &owner, t_size p1, const void *p2, t_size p2size,
                  abort_callback &abort)
    {
        packet_decoder::g_open(m_decoder, true, owner, p1, p2, p2size, abort);
    }
    t_size set_stream_property(const GUID type, t_size p1,const void * p2,
                               t_size p2size)
    {
        return m_decoder->set_stream_property(type, p1, p2, p2size);
    }
    void get_info(file_info &info)
    {
        m_decoder->get_info(info);
    }
    unsigned get_max_frame_dependency()
    {
        return m_decoder->get_max_frame_dependency();
    }
    void decode(const void *buffer, t_size bytes,
                audio_chunk &chunk, abort_callback &abort)
    {
        m_decoder->decode(buffer, bytes, chunk, abort);
    }
    void reset_after_seek()
    {
        m_decoder->reset_after_seek();
    }
    bool analyze_first_frame_supported()
    {
        return m_decoder->analyze_first_frame_supported();
    }
    void analyze_first_frame(const void *buffer, t_size bytes,
                                     abort_callback &abort)
    {
        return m_decoder->analyze_first_frame(buffer, bytes, abort);
    }
};

#endif
