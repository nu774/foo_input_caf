#include "IMA4Decoder.h"

namespace {
    const int8_t ima4_index_table[16] = {
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8,
    };
    const int16_t ima4_step_table[89] = {
            7,     8,     9,    10,    11,    12,    13,    14,    16,    17,
           19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
           50,    55,    60,    66,    73,    80,    88,    97,   107,   118,
          130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
          337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
          876,   963,  1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
         2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
         5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    };

    inline int32_t clamp(int32_t x, int32_t low, int32_t high)
    {
        return x < low ? low : x > high ? high : x;
    }
}

IMA4Decoder::IMA4Decoder(const CAFFile::Format &format): m_format(format)
{
    CAFFile::Format lpcm_format        = format;
    lpcm_format.asbd.mFormatID         = FOURCC('l','p','c','m');
    lpcm_format.asbd.mFormatFlags      = 2;
    lpcm_format.asbd.mBytesPerPacket   = 2 * format.asbd.mChannelsPerFrame;
    lpcm_format.asbd.mFramesPerPacket  = 1;
    lpcm_format.asbd.mBytesPerFrame    = lpcm_format.asbd.mBytesPerPacket;
    lpcm_format.asbd.mBitsPerChannel   = 16;
    m_lpcm_decoder = std::make_shared<LPCMDecoder>(lpcm_format);

    m_channel_state.resize(format.asbd.mChannelsPerFrame);
}

void IMA4Decoder::get_info(file_info &info)
{
    info.info_set("codec",    "IMA 4:1");
    info.info_set("encoding", "lossy");
    info.info_set_int("samplerate", m_format.asbd.mSampleRate);
    uint32_t channel_mask = m_format.channel_mask;
    std::string channels;
    if (channel_mask) {
        channels = Helpers::describe_channels(channel_mask);
        info.info_set("channels", channels.c_str());
    } else {
        info.info_set_int("channels", m_format.asbd.mChannelsPerFrame);
    }
}

void IMA4Decoder::decode(const void *buffer, t_size bytes, audio_chunk &chunk,
                         abort_callback &abort)
{
    unsigned nchannels = m_channel_state.size();
    unsigned nblocks   = bytes / nchannels / 34;
    auto bp = static_cast<const uint8_t*>(buffer);
    unsigned nsamples  = nblocks * nchannels * 64;

    m_sample_buffer.resize(nsamples);
    for (unsigned i = 0; i < nblocks; ++i) {
        for (unsigned ch = 0; ch < nchannels; ++ch) {
            decode_block(&m_channel_state[ch], &bp[(i * nchannels + ch) * 34],
                         &m_sample_buffer[i * nchannels * 64 + ch], nchannels);
        }
    }
    m_lpcm_decoder->decode(m_sample_buffer.data(), nsamples * 2, chunk, abort);
}

#define CLAMP(x, low, high) ((x)<(low)?(low):(x)>(high)?(high):(x))

void IMA4Decoder::decode_block(ChannelState *cs, const uint8_t *bp, int16_t *sp,
                               unsigned stride)
{
    int word = static_cast<int16_t>((bp[0] << 8) | bp[1]);
    int predictor  = word & ~0x7f;
    int step_index = word & 0x7f;

    if (cs->step_index != step_index
     || std::abs(cs->predictor - predictor) > 0x7f) {
        cs->predictor  = predictor;
        cs->step_index = clamp(step_index, 0, 88);
    }
    bp += 2;
    for (unsigned i = 0; i < 32; ++i, sp += stride * 2) {
        uint8_t b = *bp++;
        sp[0]      = decode_nibble(cs, b & 0xf);
        sp[stride] = decode_nibble(cs, b >> 4);
    }
}

int16_t IMA4Decoder::decode_nibble(ChannelState *cs, uint8_t nibble)
{
    int step = ima4_step_table[cs->step_index];
    int diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) diff = -diff;
    cs->predictor  = clamp(cs->predictor + diff, -32768, 32767);
    cs->step_index = clamp(cs->step_index + ima4_index_table[nibble], 0, 88);
    return cs->predictor;
}
