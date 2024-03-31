#define NOMINMAX
#include <algorithm>
#include "Decoder.h"
#include "../helpers/helpers.h"

class input_caf : public input_stubs {
    service_ptr_t<file>       m_pfile;
    std::shared_ptr<CAFFile>  m_demuxer;
    std::shared_ptr<IDecoder> m_decoder;
    int64_t                   m_current_packet;
    uint32_t                  m_start_skip;
    uint32_t                  m_packets_per_chunk;
    std::vector<uint8_t>      m_chunk_buffer;
    dynamic_bitrate_helper    m_vbr_helper;
    bool                      m_need_channel_remap;
public:
    void open(service_ptr_t<file> file, const char *path,
              t_input_open_reason reason, abort_callback &abort)
    {
        m_pfile = file;
        input_open_file_helper(m_pfile, path, reason, abort);
        m_pfile->ensure_seekable();
        m_demuxer = std::make_shared<CAFFile>(m_pfile, abort);
        m_decoder = IDecoder::create_decoder(m_demuxer, abort);
    }
    void get_info(file_info &info, abort_callback &abort)
    {
        auto asbd = m_demuxer->format().asbd;
        info.set_length(m_demuxer->duration() / asbd.mSampleRate);
        info.info_set_bitrate(m_demuxer->bitrate());
        info.info_set_int("samplerate", asbd.mSampleRate);
        uint32_t channel_mask = m_demuxer->format().channel_mask;
        std::string channels;
        if (channel_mask) {
            channels = Helpers::describe_channels(channel_mask);
            info.info_set("channels", channels.c_str());
        } else {
            info.info_set_int("channels", asbd.mChannelsPerFrame);
        }
        m_decoder->get_info(info);
        m_demuxer->get_metadata(info);
    }
    t_filestats get_file_stats(abort_callback &abort)
    {
        return m_pfile->get_stats(abort);
    }
    void decode_initialize(unsigned flags, abort_callback &abort)
    {
        auto asbd           = m_demuxer->format().asbd;
        m_current_packet    = 0;
        m_start_skip        = m_demuxer->start_offset() + decoder_delay();
        m_packets_per_chunk = 1;
        if (asbd.mBytesPerPacket > 0) {
            while (m_packets_per_chunk * asbd.mBytesPerPacket < 4096)
                m_packets_per_chunk <<= 1;
        }
        m_vbr_helper.reset();
    }
    bool decode_run(audio_chunk &chunk, abort_callback &abort)
    {
        if (m_current_packet >= m_demuxer->num_packets() + 1)
            return false;
        int64_t pull_packet = m_current_packet;
        if (m_current_packet == m_demuxer->num_packets()) {
            /*
             * If the end padding is shorter than the decoder delay,
             * we already have fed all packets to the decoder but still
             * we have to feed extra packet to pull the final delayed result.
             * Due to overlap+add, samples might not be fully reconstructed
             * anyway...
             */
            if (decoder_delay() <= m_demuxer->end_padding())
                return false;
            else
                pull_packet = m_current_packet - 1;
        }
        auto asbd = m_demuxer->format().asbd;
        uint32_t fpp  = asbd.mFramesPerPacket;
        uint32_t npackets = m_demuxer->read_packets(pull_packet,
                                                    m_packets_per_chunk,
                                                    &m_chunk_buffer, abort);
        if (npackets == 0)
            return false;
        m_current_packet += npackets;
        int64_t trim = std::max(m_current_packet * fpp - m_demuxer->duration()
                                - m_demuxer->start_offset() - decoder_delay(),
                                static_cast<int64_t>(0));
        if (trim >= fpp * npackets)
            return false;
        m_decoder->decode(m_chunk_buffer.data(), m_chunk_buffer.size(),
                          chunk, abort);
        t_size nframes = chunk.get_sample_count();
        unsigned nchannels = chunk.get_channels();
        if (trim > 0) {
            nframes = fpp * npackets - trim;
            chunk.set_sample_count(nframes);
        }
        if (m_start_skip) {
            if (m_start_skip >= nframes) {
                m_start_skip -= nframes;
                return decode_run(chunk, abort);
            }
            uint32_t rest = nframes - m_start_skip;
            uint32_t bpf  = nchannels * sizeof(audio_sample);
            audio_sample *bp = chunk.get_data();
            std::memmove(bp, bp + m_start_skip * nchannels, rest * bpf);
            chunk.set_sample_count(rest);
            m_start_skip = 0;
        }
        update_dynamic_vbr_info(m_current_packet - npackets, m_current_packet);
        return true;
    }
    bool decode_run_raw(audio_chunk &chunk, mem_block_container &raw,
                        abort_callback &abort)
    {
        throw pfc::exception_not_implemented();
    }
    void decode_seek(double seconds, abort_callback &abort)
    {
        auto asbd = m_demuxer->format().asbd;
        int64_t position = seconds * asbd.mSampleRate + .5;
        m_decoder->reset_after_seek();
        m_vbr_helper.reset();

        if (position >= m_demuxer->duration()) {
            /* let next decode_run() finish */
            m_current_packet = m_demuxer->num_packets() + 1;
            return;
        }
        uint32_t fpp       = asbd.mFramesPerPacket;
        uint32_t start_off = m_demuxer->start_offset();
        int64_t  ipacket   = (position + start_off) / fpp;
        uint32_t preroll   = m_decoder->get_max_frame_dependency();
        int64_t  ppacket   = std::max(0LL, ipacket - preroll);
        m_start_skip = position + start_off + decoder_delay() - ipacket * fpp;
        audio_chunk_impl tmp_chunk;
        if (!ipacket && m_decoder->get_max_frame_dependency())
            m_decoder = IDecoder::create_decoder(m_demuxer, abort);
        while (ppacket < ipacket) {
            m_demuxer->read_packets(ppacket++, 1, &m_chunk_buffer, abort);
            m_decoder->decode(m_chunk_buffer.data(), m_chunk_buffer.size(),
                              tmp_chunk, abort);
        }
        m_current_packet = ipacket;
    }
    bool decode_can_seek()
    {
        return m_pfile->can_seek();
    }
    bool decode_get_dynamic_info(file_info &info, double &ts_delta)
    {
        if (m_demuxer->is_cbr())
            return false;
        return m_vbr_helper.on_update(info, ts_delta);
    }
    bool decode_get_dynamic_info_track(file_info &info, double &ts_delta)
    {
        return false;
    }
    void decode_on_idle(abort_callback &abort)
    {
        m_pfile->on_idle(abort);
    }
    void retag(const file_info &info, abort_callback &abort)
    {
        m_demuxer->set_metadata(info, abort);
    }
    void remove_tags(abort_callback &abort)
    {
        m_demuxer->set_metadata(file_info_const_impl(), abort);
    }
    static bool g_is_our_content_type(const char *content_type)
    {
        return false;
    }
    static bool g_is_our_path(const char *path, const char *ext)
    {
        return !_stricmp(ext, "caf");
    }
    static const char *g_get_name() { return "CAF Decoder"; }
    static const GUID g_get_guid() {
        // {9130A8E2-B6A3-4E5C-83E6-877C32EE0EF5}
        static const GUID guid = { 0x9130a8e2, 0xb6a3, 0x4e5c,{ 0x83, 0xe6, 0x87, 0x7c, 0x32, 0xee, 0xe, 0xf5 } };
        return guid;
    }
private:
    uint32_t decoder_delay()
    {
        switch (m_demuxer->format().asbd.mFormatID) {
        case FOURCC('.','m','p','1'): return 240 + 1;
        case FOURCC('.','m','p','2'): return 240 + 1;
        case FOURCC('.','m','p','3'): return 528 + 1;
        case FOURCC('a','a','c','h'): return (480 + 1) * 2;
        case FOURCC('a','a','c','p'): return (480 + 1) * 2;
        }
        return 0;
    }
    void update_dynamic_vbr_info(uint64_t pre_packet, uint64_t cur_packet)
    {
        if (m_demuxer->is_cbr() || cur_packet >= m_demuxer->num_packets())
            return;
        auto asbd = m_demuxer->format().asbd;
        uint64_t pre_off = m_demuxer->packet_info(pre_packet);
        uint64_t cur_off = m_demuxer->packet_info(cur_packet);
        uint64_t bytes = cur_off - pre_off;
        double duration = (cur_packet - pre_packet)
            * asbd.mFramesPerPacket / asbd.mSampleRate;
        m_vbr_helper.on_frame(duration, bytes << 3);
    }
};

static input_cuesheet_factory_t<input_caf> g_input_caf_factory;
#include "input_caf.h"
