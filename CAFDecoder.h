#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdint.h>
#include "AudioFileX.h"
#include "ExtAudioFileX.h"

class IStreamReader {
public:
    virtual ~IStreamReader() {}
    virtual ssize_t read(void *data, size_t n) = 0;
    virtual int seek(int64_t off, int whence) = 0;
    virtual int64_t get_position() = 0;
    virtual int64_t get_size() = 0;
};

class CAFDecoder {
    enum { ioErr = -36 };
    AudioFileX m_iaf;
    ExtAudioFileX m_eaf;
    bool m_isCBR;
    int32_t m_bitrate;
    uint32_t m_chanmask;
    int64_t m_length;
    AudioStreamBasicDescription m_iasbd, m_oasbd;
    std::shared_ptr<AudioChannelLayout> m_channel_layout;
    std::vector<uint32_t> m_chanmap;
protected:
    std::shared_ptr<IStreamReader> m_pstream;
public:
    CAFDecoder(std::shared_ptr<IStreamReader> &pstream);
    uint64_t getLength() const
    {
	return m_length;
    }
    bool isLossless() const
    {
	return m_iasbd.mFormatID == FOURCC('l','p','c','m')
	    || m_iasbd.mFormatID == FOURCC('a','l','a','c');
    }
    uint32_t getBitrate() const
    {
	return (m_bitrate + 500) / 1000;
    }
    const AudioStreamBasicDescription &getInputFormat() const
    {
	return m_iasbd;
    }
    const AudioStreamBasicDescription &getOutputFormat() const
    {
	return m_oasbd;
    }
    uint32_t getChannelMask() const
    {
	return m_chanmask;
    }
    bool isCBR()
    {
	return m_isCBR;
    }
    int getBitsPerChannel() const;
    uint32_t readSamples(void *buffer, size_t nsamples);
    void seek(int64_t frame_offset);
    void getPacketTableInfo(AudioFilePacketTableInfo *info)
    {
	m_iaf.getPacketTableInfo(info);
    }
    uint64_t getByteOffset(uint64_t packet)
    {
	return m_iaf.getPacketToByte(packet);
    }
private:
    void retrieveChannelMap();
    bool decodeToFloat() const;
    int getDecodingBitsPerChannel() const;
    static OSStatus staticReadCallback(void *cookie, SInt64 pos, UInt32 count,
				       void *data, UInt32 *nread);
    static SInt64 staticSizeCallback(void *cookie);
    OSStatus readCallback(SInt64 pos, UInt32 count, void *data, UInt32 *nread);
    SInt64 sizeCallback();
};

