#include "CAFDecoder.h"
#include "chanmap.h"

CAFDecoder::CAFDecoder(std::shared_ptr<IStreamReader> &pstream)
    : m_pstream(pstream),
      m_chanmask(0)
{
    m_encoder_delay = 0;
    std::memset(&m_ptinfo, 0, sizeof m_ptinfo);

    AudioFileID iafid;
    m_pwriter = dynamic_cast<IStreamWriter*>(m_pstream.get());
    if (m_pwriter)
        CHECKCA(AudioFileOpenWithCallbacks(this, staticReadCallback,
                                           staticWriteCallback,
                                           staticSizeCallback,
                                           staticTruncateCallback,
                                           0, &iafid));
    else
        CHECKCA(AudioFileOpenWithCallbacks(this, staticReadCallback, 0, 
                                           staticSizeCallback, 0, 0,
                                           &iafid));
    m_iaf.attach(iafid, true);
#ifndef _DEBUG
    if (m_iaf.getFileFormat() != kAudioFileCAFType)
        throw std::runtime_error("Not a CAF file");
#endif

    std::vector<AudioFormatListItem> aflist;
    m_iaf.getFormatList(&aflist);
    m_iasbd = aflist[0].mASBD;
#ifndef _DEBUG
    if (m_iasbd.mFormatID == kAudioFormatMPEG4AAC_HE_V2)
        throw std::runtime_error("HE-AACv2 is not supported");
#endif

    m_bitrate = m_iaf.getBitrate();
    m_isCBR = (m_iasbd.mBytesPerPacket != 0);
    if (!m_isCBR) {
        uint64_t total_bytes = m_iaf.getAudioDataByteCount();
        uint64_t packet_count = m_iaf.getAudioDataPacketCount();
        uint32_t max_packet_size = m_iaf.getMaximumPacketSize();
        uint64_t upper_bound = max_packet_size * packet_count;
        uint64_t lower_bound = upper_bound - packet_count;
        m_isCBR = (lower_bound < total_bytes && total_bytes <= upper_bound);
    }

    ExtAudioFileRef eaf;
    CHECKCA(ExtAudioFileWrapAudioFileID(m_iaf, false, &eaf));
    m_eaf.attach(eaf, true);

    m_length = m_iaf.getAudioDataPacketCount() * m_iasbd.mFramesPerPacket;
    try {
        m_iaf.getPacketTableInfo(&m_ptinfo);
        int64_t total =
            m_ptinfo.mNumberValidFrames + m_ptinfo.mPrimingFrames +
            m_ptinfo.mRemainderFrames;
        if (total == m_length) {
            m_length = m_ptinfo.mNumberValidFrames;
            m_encoder_delay = m_ptinfo.mPrimingFrames;
        } else if (total == m_length / 2) {
            m_length = m_ptinfo.mNumberValidFrames * 2;
            m_encoder_delay = m_ptinfo.mPrimingFrames * 2;
        }
    } catch (CoreAudioException &e) {
        if (!e.isNotSupportedError())
            throw;
    }
    try {
        m_iaf.getChannelLayout(&m_channel_layout);
    } catch (CoreAudioException &e) {
        if (!e.isNotSupportedError())
            throw;
    }
    retrieveChannelMap();

    m_oasbd = m_iasbd;
    if (m_iasbd.mFormatID == kAudioFormatLinearPCM) {
        m_oasbd.mFormatFlags &= 0xf;
        if (m_iasbd.mBitsPerChannel & 0x7)
            m_oasbd.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
        m_oasbd.mFormatFlags &= ~kAudioFormatFlagIsBigEndian;
        if (m_iasbd.mBitsPerChannel == 8)
            m_oasbd.mFormatFlags &= ~kAudioFormatFlagIsSignedInteger;
    } else {
        m_oasbd.mFormatID = kAudioFormatLinearPCM;
        if (m_oasbd.mBitsPerChannel & 0x7)
            m_oasbd.mFormatFlags = kAudioFormatFlagIsAlignedHigh;
        else
            m_oasbd.mFormatFlags = kAudioFormatFlagIsPacked;
        if (decodeToFloat())
            m_oasbd.mFormatFlags |= kAudioFormatFlagIsFloat;
        else
            m_oasbd.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    }
    m_oasbd.mBitsPerChannel = getDecodingBitsPerChannel();
    m_oasbd.mFramesPerPacket = 1;
    m_oasbd.mBytesPerFrame = m_oasbd.mChannelsPerFrame *
        ((m_oasbd.mBitsPerChannel + 7) >> 3);
    m_oasbd.mBytesPerPacket = m_oasbd.mBytesPerFrame;
    m_eaf.setClientDataFormat(m_oasbd);
}

int CAFDecoder::getBitsPerChannel() const
{
    if (m_iasbd.mBitsPerChannel)
        return m_iasbd.mBitsPerChannel;
    else if (m_iasbd.mFormatID == kAudioFormatAppleLossless)
        return m_oasbd.mBitsPerChannel;
    else if (m_iasbd.mBytesPerPacket) {
        return static_cast<double>(m_iasbd.mBytesPerPacket << 3)
            / m_iasbd.mChannelsPerFrame / m_iasbd.mFramesPerPacket + .5;
    }
    return 0;
}

void CAFDecoder::retrieveChannelMap()
{
    if (!m_channel_layout.get())
        return;
    try {
        std::vector<uint16_t> ichannels, ochannels;
        chanmap::getChannels(m_channel_layout.get(), &ichannels);
        if (ichannels.size()) {
            chanmap::convertFromAppleLayout(ichannels, &ochannels);
            m_chanmask = chanmap::getChannelMask(ochannels);
            chanmap::getMappingToUSBOrder(ochannels, &m_chanmap);
            for (size_t i = 0; i < m_chanmap.size(); ++i)
                m_chanmap[i] -= 1;
        }
    } catch (...) {}
}

bool CAFDecoder::decodeToFloat() const
{
    return m_iasbd.mFormatID == kAudioFormatMPEG4AAC       ||
           m_iasbd.mFormatID == kAudioFormatMPEG4AAC_HE    ||
           m_iasbd.mFormatID == kAudioFormatMPEG4AAC_LD    ||
           m_iasbd.mFormatID == kAudioFormatMPEG4AAC_HE_V2 ||
           m_iasbd.mFormatID == FOURCC('p','a','a','c')    ||
           m_iasbd.mFormatID == kAudioFormatMPEGLayer1     ||
           m_iasbd.mFormatID == kAudioFormatMPEGLayer2     ||
           m_iasbd.mFormatID == kAudioFormatMPEGLayer3     ||
           m_iasbd.mFormatID == kAudioFormatAC3            ||
           m_iasbd.mFormatID == kAudioFormat60958AC3       ||
           m_iasbd.mFormatID == FOURCC('i','l','b','c');
}

int CAFDecoder::getDecodingBitsPerChannel() const
{
    if (m_iasbd.mFormatID == 'lpcm')
        return m_iasbd.mBitsPerChannel;
    else if (m_iasbd.mFormatID == kAudioFormatAppleLossless) {
        unsigned tab[] = { 16, 20, 24, 32 };
        unsigned index = (m_iasbd.mFormatFlags - 1) & 0x3;
        return tab[index];
    }
    else if (decodeToFloat())
        return 32;
    else
        return 16;
}

OSStatus CAFDecoder::readCallback(SInt64 pos, UInt32 count, void *data,
                                  UInt32 *nread)
{
    try {
        if (m_pstream->seek(pos, SEEK_SET) < 0)
            return ioErr;
        ssize_t n = m_pstream->read(data, count);
        if (n < 0)
            return ioErr;
        *nread = n;
        return 0;
    } catch (...) {
        return ioErr;
    }
}

SInt64 CAFDecoder::sizeCallback()
{
    try {
        int64_t size = m_pstream->get_size();
        return size < 0 ? ioErr : size;
    } catch (...) {
        return ioErr;
    }
}

OSStatus CAFDecoder::writeCallback(SInt64 pos, UInt32 count, const void *data,
                                   UInt32 *nwritten)
{
    try {
        if (m_pwriter->seek(pos, SEEK_SET) < 0)
            return ioErr;
        ssize_t n = m_pwriter->write(data, count);
        if (n < 0)
            return ioErr;
        *nwritten = n;
        return 0;
    } catch (...) {
        return ioErr;
    }
}

OSStatus CAFDecoder::truncateCallback(SInt64 size)
{
    try {
        return m_pwriter->set_size(size) < 0 ? ioErr : 0;
    } catch (...) {
        return ioErr;
    }
}

uint32_t CAFDecoder::readSamples(void *buffer, size_t nsamples)
{
    AudioBufferList abl = { 0 };
    abl.mNumberBuffers = 1;
    abl.mBuffers[0].mNumberChannels = m_oasbd.mChannelsPerFrame;
    abl.mBuffers[0].mData = buffer;
    abl.mBuffers[0].mDataByteSize = nsamples * m_oasbd.mBytesPerPacket;

    UInt32 ns = nsamples;
    CHECKCA(ExtAudioFileRead(m_eaf, &ns, &abl));

    if (m_chanmap.size()) {
        uint8_t *bp = reinterpret_cast<uint8_t*>(buffer);
        uint32_t bpf = m_oasbd.mBytesPerFrame;
        uint32_t bpc = bpf / m_oasbd.mChannelsPerFrame;
        uint8_t tmp[256]; // XXX: maximum: 64bit double, 32 channel
        for (size_t i = 0; i < ns; ++i, bp += bpf) {
            std::memcpy(tmp, bp, bpf);
            for (size_t j = 0; j < m_chanmap.size(); ++j)
                std::memcpy(&bp[bpc * j], &tmp[bpc * m_chanmap[j]], bpc);
        }
    }
    return ns;
}

void CAFDecoder::seek(int64_t frame_offset)
{
    int64_t preroll_offset = frame_offset;
    int preroll_packets = 0;

    switch (m_iasbd.mFormatID) {
    case kAudioFormatMPEGLayer1: preroll_packets = 1; break;
    case kAudioFormatMPEGLayer2: preroll_packets = 1; break;
    case kAudioFormatMPEGLayer3: preroll_packets = 2; break;
    }

    preroll_offset = std::max(0LL, frame_offset - m_iasbd.mFramesPerPacket *
                              preroll_packets);

    CHECKCA(ExtAudioFileSeek(m_eaf, preroll_offset));

    int64_t distance = frame_offset - preroll_offset;
    if (distance > 0) {
        size_t nbytes = distance * m_oasbd.mBytesPerFrame;
        if (m_preroll_buffer.size() < nbytes)
            m_preroll_buffer.resize(nbytes);
        readSamples(&m_preroll_buffer[0], distance);
    }
}
