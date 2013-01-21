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

class IStreamWriter: public IStreamReader {
public:
    virtual ssize_t write(const void *buffer, size_t n) = 0;
    virtual int set_size(int64_t n) = 0;
};

class CAFDecoder {
protected:
    enum { ioErr = -36 };
    AudioFileX m_iaf;
    ExtAudioFileX m_eaf;
    bool m_isCBR;
    int32_t m_bitrate;
    uint32_t m_chanmask;
    int32_t m_encoder_delay;
    int64_t m_length;
    AudioStreamBasicDescription m_iasbd, m_oasbd;
    AudioFilePacketTableInfo m_ptinfo;
    std::shared_ptr<AudioChannelLayout> m_channel_layout;
    std::vector<uint32_t> m_chanmap;
    std::shared_ptr<IStreamReader> m_pstream;
    IStreamWriter *m_pwriter;
    std::vector<uint8_t> m_preroll_buffer;
public:
    CAFDecoder(std::shared_ptr<IStreamReader> &pstream);
    ~CAFDecoder()
    {
        m_eaf.attach(0, false);
        m_iaf.close();
    }
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
    int getEncoderDelay() const
    {
        return m_encoder_delay;
    }
    int getBitsPerChannel() const;
    uint32_t readSamples(void *buffer, size_t nsamples);
    void seek(int64_t frame_offset);
    void getPacketTableInfo(AudioFilePacketTableInfo *info)
    {
        *info = m_ptinfo;
    }
    uint64_t getByteOffset(uint64_t packet)
    {
        return m_iaf.getPacketToByte(packet);
    }
    void setInfoDictionary(CFDictionaryRef dict)
    {
        if (m_pwriter)
            m_iaf.setInfoDictionary(dict);
        else
            throw std::runtime_error("CAFDecoder: not opened for writing");
    }
private:
    void retrieveChannelMap();
    bool decodeToFloat() const;
    int getDecodingBitsPerChannel() const;

    static
    OSStatus staticReadCallback(void *cookie, SInt64 pos, UInt32 count,
                                void *data, UInt32 *nread)
    {
        CAFDecoder *self = static_cast<CAFDecoder*>(cookie);
        return self->readCallback(pos, count, data, nread);
    }
    static
    SInt64 staticSizeCallback(void *cookie)
    {
        CAFDecoder *self = static_cast<CAFDecoder*>(cookie);
        return self->sizeCallback();
    }
    static
    OSStatus staticWriteCallback(void *cookie, SInt64 pos, UInt32 count,
                                 const void *data, UInt32 *nwritten)
    {
        CAFDecoder *self = static_cast<CAFDecoder*>(cookie);
        return self->writeCallback(pos, count, data, nwritten);
    }
    static
    OSStatus staticTruncateCallback(void *cookie, SInt64 size)
    {
        CAFDecoder *self = static_cast<CAFDecoder*>(cookie);
        return self->truncateCallback(size);
    }

    OSStatus readCallback(SInt64 pos, UInt32 count, void *data, UInt32 *nread);
    SInt64 sizeCallback();
    OSStatus writeCallback(SInt64 pos, UInt32 count, const void *data,
                           UInt32 *nwritten);
    OSStatus truncateCallback(SInt64 size);
};
