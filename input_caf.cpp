#include <cstring>
#include <vector>
#include <memory>
#include <stdint.h>
#include "CAFDecoder.h"
#include "CAFMetaData.h"
#include "StreamReaderImpl.h"
#include "../SDK/foobar2000.h"
#include "../helpers/helpers.h"
#include "init.h"

static const char *get_codec_name(uint32_t fcc)
{
    switch (fcc) {
    case FOURCC('.','m','p','1'):
	return "MP1";
    case FOURCC('.','m','p','2'):
	return "MP2";
    case FOURCC('.','m','p','3'):
	return "MP3";
    case FOURCC('Q','D','M','2'):
	return "QDesign Music 2";
    case FOURCC('Q','D','M','C'):
	return "QDesign";
    case FOURCC('Q','c','l','p'):
	return "Qualcomm PureVoice";
    case FOURCC('Q','c','l','q'):
	return "Qualcomm QCELP";
    case FOURCC('a','a','c',' '):
	return "AAC";
    case FOURCC('a','a','c','h'):
	return "AAC";
    case FOURCC('a','a','c','p'):
	return "AAC";
    case FOURCC('a','l','a','c'):
	return "ALAC";
    case FOURCC('a','l','a','w'):
	return "A-Law";
    case FOURCC('d','v','i','8'):
	return "DVI";
    case FOURCC('i','l','b','c'):
	return "iLBC";
    case FOURCC('i','m','a','4'):
	return "IMA 4:1";
    case FOURCC('l','p','c','m'):
	return "PCM";
    case FOURCC('m','s','\000','\002'):
	return "MS ADPCM";
    case FOURCC('m','s','\000','\021'):
	return "DVI/IMA ADPCM";
    case FOURCC('m','s','\000','1'):
	return "MS GSM 6.10";
    case FOURCC('p','a','a','c'):
	return "AAC";
    case FOURCC('s','a','m','r'):
	return "AMR-NB";
    case FOURCC('u','l','a','w'):
	return "\xc2""\xb5""-Law";
    }
    return 0;
}

class input_caf {
    service_ptr_t<file> m_pfile;
    std::shared_ptr<CAFDecoder> m_decoder;
    dynamic_bitrate_helper m_vbr_helper;
    std::vector<uint8_t> m_buffer;
    uint32_t m_encoder_delay;
    uint32_t m_pull_packets;
    uint64_t m_current_frame;
public:
    void open(service_ptr_t<file> pfile, const char *path,
	      t_input_open_reason reason, abort_callback &abort)
    {
	if (g_CoreAudioToolboxVersion.empty())
	    throw pfc::exception("CoreAudioToolbox.dll not found");
	/*
	if (reason == input_open_info_write)
	    throw exception_io_unsupported_format();
	*/
	m_pfile = pfile;
	input_open_file_helper(m_pfile, path, reason, abort);
	m_pfile->ensure_seekable();
    }
    void get_info(file_info &pinfo, abort_callback &abort)
    {
	create_decoder();

	std::shared_ptr<IStreamReader> reader(new StreamReaderImpl(m_pfile));
	CAFMetaData meta(reader);
	meta.getInfo(pinfo);

	const AudioStreamBasicDescription &asbd = m_decoder->getInputFormat();
	pinfo.set_length(m_decoder->getLength() / asbd.mSampleRate);
	pinfo.info_set_bitrate(m_decoder->getBitrate());
	pinfo.info_set_int("samplerate", asbd.mSampleRate);
	pinfo.info_set_int("channels", asbd.mChannelsPerFrame);

	int bitwidth = m_decoder->getBitsPerChannel();
	if (bitwidth > 0)
	    pinfo.info_set_int("bitspersample", bitwidth);

	pinfo.info_set("encoding",
		       m_decoder->isLossless() ? "lossless" : "lossy");
	const char *codec = get_codec_name(asbd.mFormatID);
	if (codec)
	    pinfo.info_set("codec", codec);
	if (asbd.mFormatID == FOURCC('a','a','c',' '))
	    pinfo.info_set("codec_profile", "LC");
	else if (asbd.mFormatID == FOURCC('a','a','c','h'))
	    pinfo.info_set("codec_profile", "SBR");
	else if (asbd.mFormatID == FOURCC('a','a','c','p'))
	    pinfo.info_set("codec_profile", "SBR+PS");
	else if (asbd.mFormatID == FOURCC('.','m','p','3')) {
	    pinfo.info_set("codec_profile",
		m_decoder->isCBR() ? "CBR" : "VBR");
	}
    }
    t_filestats get_file_stats(abort_callback &abort)
    {
	return m_pfile->get_stats(abort);
    }
    void decode_initialize(unsigned flags, abort_callback &abort)
    {
	m_pfile->reopen(abort);
	if (m_decoder.get())
	    m_decoder.reset();
	create_decoder();
    }
    bool decode_run(audio_chunk &chunk, abort_callback &abort)
    {
	const AudioStreamBasicDescription &asbd = m_decoder->getOutputFormat();
	uint32_t fpp = m_decoder->getInputFormat().mFramesPerPacket;
	uint32_t frame_offset_in_packet = m_current_frame % fpp;
	uint32_t nframes = m_pull_packets * fpp - frame_offset_in_packet;

	nframes = m_decoder->readSamples(&m_buffer[0], nframes);
	if (!nframes)
	    return false;
	uint32_t chanmask = m_decoder->getChannelMask();
	if (!chanmask)
	    chanmask =
		audio_chunk::g_guess_channel_config(asbd.mChannelsPerFrame);
	if (asbd.mFormatFlags & kAudioFormatFlagIsFloat) {
	    chunk.set_data_floatingpoint_ex(&m_buffer[0],
					    nframes * asbd.mBytesPerFrame,
					    asbd.mSampleRate,
					    asbd.mChannelsPerFrame,
					    (asbd.mBitsPerChannel + 7) & ~7,
					    audio_chunk::FLAG_LITTLE_ENDIAN,
					    chanmask);
	} else {
	    chunk.set_data_fixedpoint_signed(&m_buffer[0],
					     nframes * asbd.mBytesPerFrame,
					     asbd.mSampleRate,
					     asbd.mChannelsPerFrame,
					     (asbd.mBitsPerChannel + 7) & ~7,
					     chanmask);
	}
	update_dynamic_vbr_info(m_current_frame, m_current_frame + nframes);
	m_current_frame += nframes;
	return true;
    }
    bool decode_run_raw(audio_chunk &chunk, mem_block_container &raw,
			abort_callback & p_abort)
    {
	throw pfc::exception_not_implemented();
    }
    void decode_seek(double seconds, abort_callback &abort)
    {
	const AudioStreamBasicDescription &asbd = m_decoder->getInputFormat();
	int64_t frame = seconds * asbd.mSampleRate + .5;
	m_decoder->seek(frame);
	m_current_frame = m_encoder_delay + frame;
	m_vbr_helper.reset();
    }
    bool decode_can_seek()
    {
	return m_pfile->can_seek();
    }
    bool decode_get_dynamic_info(file_info &info, double &ts_delta)
    {
	if (m_decoder->isCBR())
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
	create_decoder();
	CFDictionaryPtr dict;
	meta_from_fb2k::convertToInfoDictionary(info, &dict);
	m_decoder->setInfoDictionary(dict.get());
    }
    static bool g_is_our_content_type(const char *content_type)
    {
	return false;
    }
    static bool g_is_our_path(const char *path, const char *ext)
    {
	return !_stricmp(ext, "caf");
    }
    void set_logger(event_logger::ptr ptr)
    {
    }
private:
    void create_decoder()
    {
	if (m_decoder.get())
	    return;
	std::shared_ptr<IStreamReader> reader(new StreamReaderImpl(m_pfile));
	std::shared_ptr<CAFDecoder> decoder(new CAFDecoder(reader));
	m_decoder.swap(decoder);
	initialize_buffer();

	m_current_frame = m_encoder_delay = m_decoder->getEncoderDelay();
	const AudioStreamBasicDescription &asbd = m_decoder->getInputFormat();
	m_vbr_helper.reset();
    }
    void initialize_buffer()
    {
	uint32_t fpp = m_decoder->getInputFormat().mFramesPerPacket;
	uint32_t bpf = m_decoder->getOutputFormat().mBytesPerFrame;
	uint32_t bytes_per_packet = fpp * bpf;
	m_pull_packets = 1;
	while (m_pull_packets * bytes_per_packet < 8192)
	    m_pull_packets <<= 1;
	m_buffer.resize(m_pull_packets * bytes_per_packet);
    }
    void update_dynamic_vbr_info(uint64_t prev_frame, uint64_t current_frame)
    {
	if (m_decoder->isCBR())
	    return;
	const AudioStreamBasicDescription &asbd = m_decoder->getInputFormat();
	uint64_t prev_packet = prev_frame / asbd.mFramesPerPacket;
	uint64_t current_packet = current_frame / asbd.mFramesPerPacket;
	if (current_packet <= prev_packet)
	    return;
	uint64_t prev_off = m_decoder->getByteOffset(prev_packet);
	uint64_t cur_off = m_decoder->getByteOffset(current_packet);
	uint64_t bytes = cur_off - prev_off;
	double duration = (current_packet - prev_packet)
	    * asbd.mFramesPerPacket / asbd.mSampleRate;
	m_vbr_helper.on_frame(duration, bytes << 3);
    }
};

static input_cuesheet_factory_t<input_caf> g_input_caf_factory;
#include "input_caf.h"
