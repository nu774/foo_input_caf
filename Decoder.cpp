#include <iterator>
#include <winsock2.h>
#define NOMINMAX
#include <windows.h>
#include <mmreg.h>
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
    void fill_waveformat(const AudioStreamBasicDescription &asbd,
                         ADPCMWAVEFORMAT *wformat)
    {
        wformat->wfx.nChannels       = asbd.mChannelsPerFrame;
        wformat->wfx.nSamplesPerSec  = asbd.mSampleRate + .5;
        /* XXX
         * ACM decoder sees nAvgBytesPerSec.
         * it looks like floored value is expected
         * (rounded value is rejected at least in the case of GSM 6.10
         */
        wformat->wfx.nAvgBytesPerSec = asbd.mBytesPerPacket * asbd.mSampleRate
                                     / asbd.mFramesPerPacket; // + .5;
        wformat->wfx.nBlockAlign     = asbd.mBytesPerPacket;
        wformat->wSamplesPerBlock    = asbd.mFramesPerPacket;
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
    case FOURCC('f','l','a','c'):
        {
            std::vector<uint8_t> cookie;
            demuxer->get_magic_cookie(&cookie);
            packet_decoder::matroska_setup setup = { 0 };
            setup.codec_id           = "A_FLAC";
            setup.sample_rate        = asbd.mSampleRate;
            setup.channels           = asbd.mChannelsPerFrame;
            setup.codec_private      = cookie.data();
            setup.codec_private_size = cookie.size();
            decoder = MP(packet_decoder::owner_matroska, 0, &setup, sizeof(setup), abort);
            break;
        }
    case FOURCC('a','l','a','w'):
    case FOURCC('u','l','a','w'):
    case FOURCC('m','s','\0','\x02'):
    case FOURCC('m','s','\0','\x11'):
    case FOURCC('m','s','\0','1'):
        {
            std::vector<uint8_t> vec(sizeof(ADPCMWAVEFORMAT)+
                                     sizeof(ADPCMCOEFSET)*7);
            ADPCMWAVEFORMAT *wformat =
                reinterpret_cast<ADPCMWAVEFORMAT *>(vec.data());
            fill_waveformat(asbd, wformat);

            switch (asbd.mFormatID) {
            case FOURCC('a','l','a','w'):
                wformat->wfx.wFormatTag     = 0x06;
                wformat->wfx.wBitsPerSample = 8;
                wformat->wfx.cbSize         = 0;
                break;
            case FOURCC('u','l','a','w'):
                wformat->wfx.wFormatTag     = 0x07;
                wformat->wfx.wBitsPerSample = 8;
                wformat->wfx.cbSize         = 0;
                break;
            case FOURCC('m','s','\0','\x11'):
                wformat->wfx.wFormatTag     = 0x11;
                wformat->wfx.wBitsPerSample = 4;
                wformat->wfx.cbSize         = 2;
                break;
            case FOURCC('m','s','\0','1'):
                wformat->wfx.wFormatTag = 0x31;
                wformat->wfx.cbSize     = 2;
                break;
            case FOURCC('m','s','\0','\x02'):
                {
                    wformat->wfx.wFormatTag     = 0x02;
                    wformat->wfx.wBitsPerSample = 4;
                    wformat->wfx.cbSize         = 32;
                    wformat->wNumCoef           = 7;
                    const short coef1[] = { 64, 128, 0, 48, 60, 115, 98 };
                    const short coef2[] = { 0, -64, 0, 16, 0, -52, -58 };
                    for (int i = 0; i < 7; ++i) {
                        wformat->aCoef[i].iCoef1 = coef1[i] * 4;
                        wformat->aCoef[i].iCoef2 = coef2[i] * 4;
                    }
                    break;
                }
            }
            packet_decoder::matroska_setup setup = { 0 };
            setup.codec_id           = "A_MS/ACM";
            setup.sample_rate        = wformat->wfx.nSamplesPerSec;
            setup.channels           = wformat->wfx.nChannels;
            setup.codec_private      = wformat;
            setup.codec_private_size =
                sizeof(WAVEFORMATEX) + wformat->wfx.cbSize;
            decoder = MP(packet_decoder::owner_matroska, 0, &setup,
                         sizeof(setup), abort);
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

