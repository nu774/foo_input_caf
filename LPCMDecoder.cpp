#include "LPCMDecoder.h"

void LPCMDecoder::get_info(file_info &info)
{
    if (m_format.asbd.mFormatFlags & 1)
        info.info_set("codec", "PCM (floating point)");
    else
        info.info_set("codec", "PCM");
    info.info_set("encoding", "lossless");
    info.info_set_int("samplerate",    m_format.asbd.mSampleRate    );
    info.info_set_int("bitspersample", m_format.asbd.mBitsPerChannel);
    uint32_t channel_mask = m_format.channel_mask;
    std::string channels;
    if (channel_mask) {
        channels = Helpers::describe_channels(channel_mask);
        info.info_set("channels", channels.c_str());
    } else {
        info.info_set_int("channels", m_format.asbd.mChannelsPerFrame);
    }
}

void LPCMDecoder::decode(const void *buffer, t_size bytes, audio_chunk &chunk,
                         abort_callback &abort)
{
    auto     asbd      = m_format.asbd;
    unsigned channels  = asbd.mChannelsPerFrame;
    unsigned bpc       = asbd.mBytesPerPacket * 8 / channels;
    unsigned flags     = audio_chunk::FLAG_SIGNED;
    unsigned chanmask  = m_format.channel_mask;

    if (asbd.mFormatFlags & 2)
        flags |= audio_chunk::FLAG_LITTLE_ENDIAN;
    else
        flags |= audio_chunk::FLAG_BIG_ENDIAN;
    if (!chanmask)
        chanmask = audio_chunk::g_guess_channel_config(channels);
    if (asbd.mFormatFlags & 1)
        chunk.set_data_floatingpoint_ex(buffer, bytes, asbd.mSampleRate,
                                        channels, bpc, flags, chanmask);
    else
        chunk.set_data_fixedpoint_ex(buffer, bytes, asbd.mSampleRate,
                                     channels, bpc, flags, chanmask);
    if (m_need_channel_remap) {
        audio_sample *dp = chunk.get_data(),
                     *endp = dp + chunk.get_sample_count() * channels;
        std::vector<audio_sample> work(channels);
        const char *chanmap = m_format.channel_map.data();
        for (; dp < endp; dp += channels) {
            std::memcpy(work.data(), dp, channels * sizeof(audio_sample));
            for (unsigned i = 0; i < channels; ++i)
                dp[i] = work[chanmap[i]];
        }
    }
}
