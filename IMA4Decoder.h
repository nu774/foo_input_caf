#ifndef IMA4DECODER_H
#define IMA4DECODER_H

#include "LPCMDecoder.h"

class IMA4Decoder: public DecoderBase {
    struct ChannelState {
        int predictor;
        int step_index;
        ChannelState(): predictor(0), step_index(0) {}
    };
    CAFFile::Format              m_format;
    std::shared_ptr<LPCMDecoder> m_lpcm_decoder;
    std::vector<ChannelState>    m_channel_state;
    std::vector<int16_t>         m_sample_buffer;
public:
    IMA4Decoder(const CAFFile::Format &format);
    void get_info(file_info &info);
    void decode(const void *buffer, t_size bytes, audio_chunk &chunk,
                abort_callback &abort);
private:
    void decode_block(ChannelState *cs, const uint8_t *bp, int16_t *sp,
                      unsigned stride);
    int16_t decode_nibble(ChannelState *cs, uint8_t nibble);
};

#endif
