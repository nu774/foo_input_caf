#include <iterator>
#include "Decoder.h"
#include "LPCMDecoder.h"
#include "IMA4Decoder.h"
#include "PacketDecoder.h"

namespace {
    void check_aac_analyzed_info(std::shared_ptr<CAFFile>  &demuxer,
                                 std::shared_ptr<IDecoder> &decoder)
    {
        auto asbd = demuxer->format().asbd;
        file_info_impl finfo;
        decoder->get_info(finfo);
        int sample_rate     = finfo.info_get_int("samplerate");
        int nchannels       = finfo.info_get_int("channels");
        const char *profile = finfo.info_get("codec_profile");

        if (sample_rate != asbd.mSampleRate
         || nchannels   != asbd.mChannelsPerFrame) {
            if (!std::strcmp(profile, "LC"))
                asbd.mFormatID = FOURCC('a','a','c',' ');
            else if (!std::strcmp(profile, "SBR"))
                asbd.mFormatID = FOURCC('a','a','c','h');
            else if (!std::strcmp(profile, "SBR+PS"))
                asbd.mFormatID = FOURCC('a','a','c','p');
            asbd.mFramesPerPacket *= sample_rate / asbd.mSampleRate;
            asbd.mSampleRate = sample_rate;
            asbd.mChannelsPerFrame = nchannels;
            demuxer->update_format(asbd);
        }
    }
}

std::shared_ptr<IDecoder>
IDecoder::create_decoder(std::shared_ptr<CAFFile> &demuxer,
                         abort_callback &abort)
{
    auto asbd = demuxer->format().asbd;
    std::shared_ptr<IDecoder> decoder;
    bool is_aac = false;

#define MP std::make_shared<PacketDecoder>
    switch (asbd.mFormatID) {
    case FOURCC('l','p','c','m'):
        decoder = std::make_shared<LPCMDecoder>(demuxer->format());
        break;
    case FOURCC('i','m','a','4'):
        decoder = std::make_shared<IMA4Decoder>(demuxer->format());
        break;
    case FOURCC('.','m','p','1'):
        decoder = MP(packet_decoder::owner_MP1, 0, nullptr, 0, abort);
        break;
    case FOURCC('.','m','p','2'):
        decoder = MP(packet_decoder::owner_MP2, 0, nullptr, 0, abort);
        break;
    case FOURCC('.','m','p','3'):
        decoder = MP(packet_decoder::owner_MP3, 0, nullptr, 0, abort);
        break;
    case FOURCC('a','a','c',' '):
    case FOURCC('a','a','c','h'):
    case FOURCC('a','a','c','p'):
        is_aac = true;
        /* FALLTHROUGH */
    case FOURCC('a','l','a','c'):
        {
            std::vector<uint8_t> asc;
            demuxer->get_magic_cookie(&asc);
            GUID owner = is_aac ? packet_decoder::owner_MP4
                                : packet_decoder::owner_MP4_ALAC;
            decoder = MP(owner, 0x40, asc.data(), asc.size(), abort);
            break;
        }
    default:
        throw std::runtime_error("audio codec not supported");
    }
#undef MP
    if (decoder->analyze_first_frame_supported()) {
        std::vector<uint8_t> tmp_buffer;
        demuxer->read_packets(0, 1, &tmp_buffer, abort);
        decoder->analyze_first_frame(tmp_buffer.data(),
                                     tmp_buffer.size(), abort);
    }
    if (is_aac)
        check_aac_analyzed_info(demuxer, decoder);
    return decoder;
}

