#include <algorithm>
#include <numeric>
#include <iterator>
#define NOMINMAX
#include "CAFFile.h"

namespace {
    void translate_channel_labels(char *channels)
    {
        unsigned i;
        char *has_side = std::strpbrk(channels, "\x0A\x0B");

        for (i = 0; channels[i]; ++i) {
            switch (channels[i]) {
            case kAudioChannelLabel_LeftSurround:
            case kAudioChannelLabel_RightSurround:
                if (!has_side) channels[i] += 5; // map to SL/SR
                break;
            case kAudioChannelLabel_RearSurroundLeft:
            case kAudioChannelLabel_RearSurroundRight:
                if (!has_side) channels[i] -= 28; // map to BL/BR
                break;
            case kAudioChannelLabel_Mono:
                channels[i] = kAudioChannelLabel_Center;
                break;
            }
        }
    }

    unsigned read_ber_integer(stream_reader *reader, abort_callback &abort)
    {
        unsigned n = 0;
        uint8_t  b = 0;
        do {
            reader->read_object_t(b, abort);
            n <<= 7;
            n |=  b & 0x7f;
        } while (b & 0x80);
        return n;
    }

    template <typename InputIterator>
    unsigned read_ber_integer(InputIterator &begin, InputIterator end)
    {
        unsigned n = 0;
        uint8_t  b = 0x80;
        while (b >> 7 &&  begin != end) {
            b = *begin++;
            n <<= 7;
            n |=  b & 0x7f;
        }
        return n;
    }

    void get_ASC_from_magic_cookie(const std::vector<uint8_t> &cookie,
                                   std::vector<uint8_t> *asc)
    {
        std::vector<uint8_t>::const_iterator p = cookie.begin();

        while (p < cookie.end()) {
            uint8_t  tag  = *p++;
            unsigned size = read_ber_integer(p, cookie.end());
            switch (tag) {
            case 3:
                {
                    p += 2; // ES_ID
                    uint8_t flags = *p++;
                    if (flags >> 7) // streamDependenceFlag
                        p += 2;
                    if ((flags >> 6) & 0x1) { // URL_flag
                        uint8_t urllen = *p++;
                        p += urllen;
                    }
                    if ((flags >> 5) & 0x1) // OCRstreamFlag
                        p += 2;
                    break;
                }
            case 4:
                p += 13;
                break;
            case 5:
                {
                    std::vector<uint8_t> v(size);
                    std::copy(p, p + size, v.begin());
                    asc->swap(v);
                    return;
                }
            default:
                p += size;
            }
        }
        throw std::runtime_error("AudioSpecificConfig not found in kuki");
    }
}

void CAFFile::get_magic_cookie(std::vector<uint8_t> *data) const
{
    std::vector<uint8_t> res;

    switch (format().asbd.mFormatID) {
    case FOURCC('a','a','c',' '):
    case FOURCC('a','a','c','h'):
    case FOURCC('a','a','c','p'):
        get_ASC_from_magic_cookie(m_magic_cookie, &res);
        break;
    case FOURCC('a','l','a','c'):
        {
            res.resize(4);
            if (m_magic_cookie.size() > 24
             && std::memcmp(&m_magic_cookie[4], "frmaalac", 8) == 0)
                std::copy(m_magic_cookie.begin() + 24, m_magic_cookie.end(),
                          std::back_inserter(res));
            else
                std::copy(m_magic_cookie.begin(), m_magic_cookie.end(),
                          std::back_inserter(res));
            break;
        }
    default:
        res = m_magic_cookie;
    }
    data->swap(res);
}

int64_t CAFFile::packet_info(int64_t index, uint32_t *size) const
{
    int64_t  offset;

    if (index >= num_packets())
        throw std::runtime_error("invalid packet offset");

    if (m_packet_table.size() == 0) {
        auto asbd = format().asbd;
        offset = index * asbd.mBytesPerPacket;
        if (size) *size = asbd.mBytesPerPacket;
    } else {
        offset = m_packet_table[index].mStartOffset;
        if (size) *size = m_packet_table[index].mDataByteSize;
    }
    return offset;
}

uint32_t CAFFile::read_packets(int64_t offset, uint32_t count,
                               std::vector<uint8_t> *data,
                               abort_callback &abort)
{
    count = std::max(std::min(offset + count, num_packets()) - offset,
                     static_cast<int64_t>(0));
    if (count == 0)
        data->resize(0);
    else {
        uint32_t size;
        int64_t bytes_offset = packet_info(offset, &size);
        uint32_t size_total = size;
        if (m_packet_table.size() == 0)
            size_total *= count;
        else {
            for (uint32_t i = 1; i < count; ++i)
                size_total += m_packet_table[offset + i].mDataByteSize;
        }
        data->resize(size_total);
        m_pfile->seek(m_data_offset + bytes_offset, abort);
        m_pfile->read(data->data(), data->size(), abort);
    }
    return count;
}

void CAFFile::set_metadata(const file_info &info, abort_callback &abort)
{
    Metadata::put_entries(&m_tags, info);
    auto lambda = [](unsigned n, const Metadata::string_pair &e) -> unsigned {
        return n + e.first.size() + e.second.size() + 2;
    };
    int len = std::accumulate(m_tags.begin(), m_tags.end(), 0u, lambda) + 4;

    t_filesize room_pos, info_pos;
    int64_t room = find_room_for_info(&room_pos, &info_pos, abort);
    bool not_enough = (len != room && len > room - 12);
    if (not_enough) {
        room     = len;
        room_pos = m_pfile->get_size(abort);
    }
    /*
     * DESTRUCTIVE change!
     * Order is important to minimize the possibility of completely
     * destroying the box structure and making the file unreadable.
     */
    abort_callback_dummy noabort;
    /* when room was not enough, create room at the end */
    if (not_enough) {
        m_pfile->seek(room_pos, abort);
        m_pfile->write_bendian_t(FOURCC('f','r','e','e'), noabort);
        m_pfile->write_bendian_t(static_cast<int64_t>(len), noabort);
    }
    /* write out the tags */
    m_pfile->seek(room_pos + 12, noabort);
    m_pfile->write_bendian_t(static_cast<uint32_t>(m_tags.size()), noabort);
    for (size_t i = 0; i < m_tags.size(); ++i) {
        auto key = m_tags[i].first;
        auto val = m_tags[i].second;
        m_pfile->write(key.c_str(), key.size() + 1, noabort);
        m_pfile->write(val.c_str(), val.size() + 1, noabort);
    }
    /* when room was enough, cut off the remainder by free box */
    if (len < room) {
        int64_t free_size = room - len - 12;
        m_pfile->write_bendian_t(FOURCC('f','r','e','e'), noabort);
        m_pfile->write_bendian_t(free_size, noabort);
    }
    /* now we mark new info box */
    m_pfile->seek(room_pos, noabort);
    m_pfile->write_bendian_t(FOURCC('i','n','f','o'), noabort);
    m_pfile->write_bendian_t(static_cast<int64_t>(len), noabort);
    /* turn old info into free box if it was not available */
    if (info_pos > 0 && (info_pos != room_pos || not_enough)) {
        m_pfile->seek(info_pos, noabort);
        m_pfile->write_bendian_t(FOURCC('f','r','e','e'), noabort);
    }
}

void CAFFile::parse(abort_callback &abort)
{
    uint32_t fcc;
    int64_t  size;

    m_pfile->read_bendian_t(fcc, abort);
    if (fcc != FOURCC('c','a','f','f'))
        throw std::runtime_error("not a caf file");
    m_pfile->skip(4, abort);  /* mFileVersion, FileFlags */

    for (;;) {
        t_filesize pos = m_pfile->get_position(abort);
        if (pos >= m_pfile->get_size(abort))
            break;
        m_pfile->read_bendian_t(fcc,  abort);
        m_pfile->read_bendian_t(size, abort);

        switch (fcc) {
        case FOURCC('d','e','s','c'):
            parse_desc(&m_primary_format, abort); break;
        case FOURCC('c','h','a','n'):
            parse_chan(&m_primary_format, abort); break;
        case FOURCC('l','d','s','c'):
            parse_ldsc(size, abort); break;
        case FOURCC('k','u','k','i'):
            parse_kuki(size, abort); break;
        case FOURCC('i','n','f','o'):
            parse_info(size, abort); break;
        case FOURCC('p','a','k','t'):
            parse_pakt(size, abort); break;
        }
        if (fcc == FOURCC('d','a','t','a')) {
            m_data_offset = pos + 16;
            m_data_size   = size - 4;
            if (size == -1) {
                m_data_size = m_pfile->get_size(abort) - pos - 16;
                break;
            }
        }
        m_pfile->seek(pos + 12 + size, abort);
    }
    if (m_primary_format.asbd.mFormatID == 0)
        throw std::runtime_error("desc chunk not found");
    if (m_data_offset == 0)
        throw std::runtime_error("data chunk not found");
    calc_duration();
}

void CAFFile::parse_desc(Format *d, abort_callback &abort)
{
    m_pfile->read_bendian_t(d->asbd.mSampleRate,       abort);
    m_pfile->read_bendian_t(d->asbd.mFormatID,         abort);
    m_pfile->read_bendian_t(d->asbd.mFormatFlags,      abort);
    m_pfile->read_bendian_t(d->asbd.mBytesPerPacket,   abort);
    m_pfile->read_bendian_t(d->asbd.mFramesPerPacket,  abort);
    m_pfile->read_bendian_t(d->asbd.mChannelsPerFrame, abort);
    m_pfile->read_bendian_t(d->asbd.mBitsPerChannel,   abort);
    d->asbd.mBytesPerFrame = d->asbd.mBytesPerPacket / d->asbd.mFramesPerPacket;

    d->channel_map.resize(d->asbd.mChannelsPerFrame);
    for (unsigned i = 0; i < d->asbd.mChannelsPerFrame; ++i)
        d->channel_map[i] = i;
}

void CAFFile::parse_chan(Format *d, abort_callback &abort)
{
    uint32_t mChannelLayoutTag;
    uint32_t mChannelBitmap;
    uint32_t mNumberChannelDescriptions;
    uint32_t mChannelLabel;
    std::vector<char> channels;
    const char *layout = 0;

    m_pfile->read_bendian_t(mChannelLayoutTag, abort);
    m_pfile->read_bendian_t(mChannelBitmap, abort);
    m_pfile->read_bendian_t(mNumberChannelDescriptions, abort);

    switch (mChannelLayoutTag) {
    case kAudioChannelLayoutTag_UseChannelBitmap:
        d->channel_mask = mChannelBitmap;
        if (Helpers::bitcount(mChannelBitmap) != d->asbd.mChannelsPerFrame)
            throw std::runtime_error("invalid channel bitmap");
        break;
    case kAudioChannelLayoutTag_UseChannelDescriptions:
        for (unsigned i = 0; i < mNumberChannelDescriptions; ++i) {
            m_pfile->read_bendian_t(mChannelLabel, abort);
            m_pfile->skip(16, abort);
            channels.push_back(mChannelLabel & 0xff);
        }
        channels.push_back(0);
        translate_channel_labels(channels.data());
        channels.pop_back();
        for (auto it = channels.begin(); it != channels.end(); ++it)
            if (*it > kAudioChannelLabel_TopBackLeft)
                throw std::runtime_error("unsupported channel layout");
        parse_channels(d, channels);
        break;
    default:
        parse_channel_layout_tag(d, mChannelLayoutTag);
        break;
    }
    if (mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions
     && mNumberChannelDescriptions > 0)
        m_pfile->skip(mNumberChannelDescriptions * 20, abort);
}

void CAFFile::parse_ldsc(int64_t size, abort_callback &abort)
{
    t_filesize end = m_pfile->get_position(abort) + size;
    while (m_pfile->get_position(abort) < end) {
        Format d;
        parse_desc(&d, abort);
        uint32_t mChannelLayoutTag;
        m_pfile->read_bendian_t(mChannelLayoutTag, abort);
        parse_channel_layout_tag(&d, mChannelLayoutTag);
        m_layered_formats.push_back(d);
    }
}

void CAFFile::parse_kuki(int64_t size, abort_callback &abort)
{
    m_magic_cookie.resize(size);
    m_pfile->read(m_magic_cookie.data(), size, abort);
}

void CAFFile::parse_info(int64_t size, abort_callback &abort)
{
    if (size <= 4 || size > 1024 * 1024 * 64) {
        m_pfile->skip(size, abort);
        return;
    }
    uint32_t num_info;
    std::vector<char> buf(size - 4);
    char *key, *val, *end;

    m_pfile->read_bendian_t(num_info, abort);
    m_pfile->read(buf.data(), size - 4, abort);

    key = buf.data();
    end = buf.data() + buf.size();
    do {
        if ((val = key + strlen(key) + 1) < end) {
            m_tags.push_back(std::make_pair(std::string(key),
                                                std::string(val)));
            key = val + strlen(val) + 1;
        }
    } while (key < end && val < end);
}

void CAFFile::parse_pakt(int64_t size, abort_callback &abort)
{
    // CAFPacketTableHeader
    int64_t i, mNumberPackets, pos = 0;
    AudioStreamBasicDescription asbd = format().asbd;

    m_pfile->read_bendian_t(mNumberPackets,                   abort);
    m_pfile->read_bendian_t(m_packet_info.mNumberValidFrames, abort);
    m_pfile->read_bendian_t(m_packet_info.mPrimingFrames,     abort);
    m_pfile->read_bendian_t(m_packet_info.mRemainderFrames,   abort);

    uint32_t low = ~0, high = 0;

    for (i = 0; i < mNumberPackets; ++i) {
        AudioStreamPacketDescription aspd = { 0 };
        aspd.mStartOffset  = pos;
        if (!asbd.mBytesPerPacket)
            aspd.mDataByteSize =
                read_ber_integer(m_pfile.get_ptr(), abort);
        else
            aspd.mDataByteSize = asbd.mBytesPerPacket;
        pos += aspd.mDataByteSize;
        if (low  > aspd.mDataByteSize) low  = aspd.mDataByteSize;
        if (high < aspd.mDataByteSize) high = aspd.mDataByteSize;

        if (!asbd.mFramesPerPacket)
            aspd.mVariableFramesInPacket =
                read_ber_integer(m_pfile.get_ptr(), abort);
        else
            aspd.mVariableFramesInPacket = asbd.mFramesPerPacket;
        m_packet_table.push_back(aspd);
    }
    if (high > low + 1)
        m_nearly_cbr = false;
}

void CAFFile::calc_duration()
{
    AudioStreamBasicDescription asbd = format().asbd;
    if (m_packet_info.mNumberValidFrames)
        m_duration = m_packet_info.mNumberValidFrames * tscale() + .5;
    else if (!m_packet_table.size())
        m_duration = m_data_size / asbd.mBytesPerPacket * asbd.mFramesPerPacket;
    else if (asbd.mFramesPerPacket)
        m_duration = m_packet_table.size() * asbd.mFramesPerPacket;
    else
        throw std::runtime_error("variable frame length is not supported");
}

void CAFFile::parse_channel_layout_tag(Format *d, uint32_t tag)
{
    const char *layout = 0;
    switch (tag) {
    /* 1ch */
    case kAudioChannelLayoutTag_Mono:
        layout = "\x03"; break;
    /* 1.1ch */
    case kAudioChannelLayoutTag_AC3_1_0_1:
        layout = "\x03\x04"; break;
    /* 2ch */
    case kAudioChannelLayoutTag_Stereo:
    case kAudioChannelLayoutTag_MatrixStereo:
    case kAudioChannelLayoutTag_Binaural:
        layout = "\x01\x02"; break;
    /* 2.1ch */
    case kAudioChannelLayoutTag_DVD_4:
        layout = "\x01\x02\x04"; break;
    /* 3ch */
    case kAudioChannelLayoutTag_MPEG_3_0_A:
        layout = "\x01\x02\x03"; break;
    case kAudioChannelLayoutTag_AC3_3_0:
        layout = "\x01\x03\x02"; break;
    case kAudioChannelLayoutTag_MPEG_3_0_B:
        layout = "\x03\x01\x02"; break;
    case kAudioChannelLayoutTag_ITU_2_1:
        layout = "\x01\x02\x09"; break;
    /* 3.1ch */
    case kAudioChannelLayoutTag_DVD_10:
        layout = "\x01\x02\x03\x04"; break;
    case kAudioChannelLayoutTag_AC3_3_0_1:
        layout = "\x01\x03\x02\x04"; break;
    case kAudioChannelLayoutTag_DVD_5:
        layout = "\x01\x02\x04\x09"; break;
    case kAudioChannelLayoutTag_AC3_2_1_1:
        layout = "\x01\x02\x09\x04"; break;
    case kAudioChannelLayoutTag_DTS_3_1:
        layout = "\x03\x01\x02\x04"; break;
    /* 4ch */
    case kAudioChannelLayoutTag_Quadraphonic:
    case kAudioChannelLayoutTag_ITU_2_2:
        layout = "\x01\x02\x0A\x0B"; break;
    case kAudioChannelLayoutTag_MPEG_4_0_A:
        layout = "\x01\x02\x03\x09"; break;
    case kAudioChannelLayoutTag_MPEG_4_0_B:
        layout = "\x03\x01\x02\x09"; break;
    case kAudioChannelLayoutTag_AC3_3_1:
        layout = "\x01\x03\x02\x09"; break;
    case kAudioChannelLayoutTag_WAVE_4_0_B:
        layout = "\x01\x02\x05\x06"; break;
    case kAudioChannelLayoutTag_Logic_4_0_C:
        layout = "\x01\x02\x09\x03"; break;
    /* 4.1ch */
    case kAudioChannelLayoutTag_DVD_6:
        layout = "\x01\x02\x04\x0A\x0B"; break;
    case kAudioChannelLayoutTag_DVD_18:
        layout = "\x01\x02\x0A\x0B\x04"; break;
    case kAudioChannelLayoutTag_DVD_11:
        layout = "\x01\x02\x03\x04\x09"; break;
    case kAudioChannelLayoutTag_AC3_3_1_1:
        layout = "\x01\x03\x02\x09\x04"; break;
    case kAudioChannelLayoutTag_DTS_4_1:
        layout = "\x03\x01\x02\x09\x04"; break;
    /* 5ch */
    case kAudioChannelLayoutTag_MPEG_5_0_A:
        layout = "\x01\x02\x03\x0A\x0B"; break;
    case kAudioChannelLayoutTag_Pentagonal:
    case kAudioChannelLayoutTag_MPEG_5_0_B:
        layout = "\x01\x02\x0A\x0B\x03"; break;
    case kAudioChannelLayoutTag_MPEG_5_0_C:
        layout = "\x01\x03\x02\x0A\x0B"; break;
    case kAudioChannelLayoutTag_MPEG_5_0_D:
        layout = "\x03\x01\x02\x0A\x0B"; break;
    case kAudioChannelLayoutTag_WAVE_5_0_B:
        layout = "\x01\x02\x03\x05\x06"; break;
    /* 5.1ch */
    case kAudioChannelLayoutTag_MPEG_5_1_A:
        layout = "\x01\x02\x03\x04\x0A\x0B"; break;
    case kAudioChannelLayoutTag_MPEG_5_1_B:
        layout = "\x01\x02\x0A\x0B\x03\x04"; break;
    case kAudioChannelLayoutTag_MPEG_5_1_C:
        layout = "\x01\x03\x02\x0A\x0B\x04"; break;
    case kAudioChannelLayoutTag_MPEG_5_1_D:
        layout = "\x03\x01\x02\x0A\x0B\x04"; break;
    case kAudioChannelLayoutTag_WAVE_5_1_B:
        layout = "\x01\x02\x03\x04\x05\x06"; break;
    /* 6ch */
    case kAudioChannelLayoutTag_Hexagonal:
    case kAudioChannelLayoutTag_AudioUnit_6_0:
        layout = "\x01\x02\x0A\x0B\x03\x09"; break;
    case kAudioChannelLayoutTag_AAC_6_0:
        layout = "\x03\x01\x02\x0A\x0B\x09"; break;
    case kAudioChannelLayoutTag_EAC_6_0_A:
        layout = "\x01\x03\x02\x0A\x0B\x09"; break;
    case kAudioChannelLayoutTag_DTS_6_0_A:
        layout = "\x07\x08\x01\x02\x0A\x0B"; break;
    case kAudioChannelLayoutTag_DTS_6_0_B:
        layout = "\x03\x01\x02\x05\x06\x0C"; break;
    case kAudioChannelLayoutTag_DTS_6_0_C:
        layout = "\x03\x09\x01\x02\x05\x06"; break;
    case kAudioChannelLayoutTag_Logic_6_0_B:
        layout = "\x01\x02\x0A\x0B\x09\x03"; break;
    /* 6.1ch */
    case kAudioChannelLayoutTag_MPEG_6_1_A:
        layout = "\x01\x02\x03\x04\x0A\x0B\x09"; break;
    case kAudioChannelLayoutTag_AAC_6_1:
        layout = "\x03\x01\x02\x0A\x0B\x09\x04"; break;
    case kAudioChannelLayoutTag_EAC3_6_1_A:
        layout = "\x01\x03\x02\x0A\x0B\x04\x09"; break;
    case kAudioChannelLayoutTag_EAC3_6_1_B:
        layout = "\x01\x03\x02\x0A\x0B\x04\x0C"; break;
    case kAudioChannelLayoutTag_EAC3_6_1_C:
        layout = "\x01\x03\x02\x0A\x0B\x04\x0E"; break;
    case kAudioChannelLayoutTag_DTS_6_1_A:
        layout = "\x07\x08\x01\x02\x0A\x0B\x04"; break;
    case kAudioChannelLayoutTag_DTS_6_1_B:
        layout = "\x03\x01\x02\x05\x06\x0C\x04"; break;
    case kAudioChannelLayoutTag_DTS_6_1_C:
        layout = "\x03\x09\x01\x02\x05\x06\x04"; break;
    case kAudioChannelLayoutTag_DTS_6_1_D:
        layout = "\x03\x01\x02\x0A\x0B\x04\x09"; break;
    case kAudioChannelLayoutTag_WAVE_6_1:
        layout = "\x01\x02\x03\x04\x09\x0A\x0B"; break;
    case kAudioChannelLayoutTag_Logic_6_1_B:
        layout = "\x01\x02\x0A\x0B\x09\x03\x04"; break;
    case kAudioChannelLayoutTag_Logic_6_1_D:
        layout = "\x01\x03\x02\x0A\x09\x0B\x04"; break;
    /* 7ch */
    case kAudioChannelLayoutTag_AudioUnit_7_0:
        layout = "\x01\x02\x0A\x0B\x03\x05\x06"; break;
    case kAudioChannelLayoutTag_AudioUnit_7_0_Front:
        layout = "\x01\x02\x0A\x0B\x03\x07\x08"; break;
    case kAudioChannelLayoutTag_AAC_7_0:
        layout = "\x03\x01\x02\x0A\x0B\x05\x06"; break;
    case kAudioChannelLayoutTag_EAC_7_0_A:
        layout = "\x01\x03\x02\x0A\x0B\x05\x06"; break;
    case kAudioChannelLayoutTag_DTS_7_0:
        layout = "\x07\x03\x08\x01\x02\x0A\x0B"; break;
    /* 7.1ch */
    case kAudioChannelLayoutTag_MPEG_7_1_A:
        layout = "\x01\x02\x03\x04\x0A\x0B\x07\x08"; break;
    case kAudioChannelLayoutTag_MPEG_7_1_B:
        layout = "\x03\x07\x08\x01\x02\x0A\x0B\x04"; break;
    case kAudioChannelLayoutTag_MPEG_7_1_C:
        layout = "\x01\x02\x03\x04\x0A\x0B\x05\x06"; break;
    case kAudioChannelLayoutTag_Emagic_Default_7_1:
        layout = "\x01\x02\x0A\x0B\x03\x04\x07\x08"; break;
    case kAudioChannelLayoutTag_AAC_7_1_B:
        layout = "\x03\x01\x02\x0A\x0B\x05\x06\x04"; break;
    case kAudioChannelLayoutTag_AAC_7_1_C:
        layout = "\x03\x01\x02\x0A\x0B\x04\x0D\x0F"; break;
    case kAudioChannelLayoutTag_EAC3_7_1_A:
        layout = "\x01\x03\x02\x0A\x0B\x04\x05\x06"; break;
    case kAudioChannelLayoutTag_EAC3_7_1_B:
        layout = "\x01\x03\x02\x0A\x0B\x04\x07\x08"; break;
    case kAudioChannelLayoutTag_EAC3_7_1_E:
        layout = "\x01\x03\x02\x0A\x0B\x04\x0D\x0F"; break;
    case kAudioChannelLayoutTag_EAC3_7_1_F:
        layout = "\x01\x03\x02\x0A\x0B\x04\x09\x0C"; break;
    case kAudioChannelLayoutTag_EAC3_7_1_G:
        layout = "\x01\x03\x02\x0A\x0B\x04\x09\x0E"; break;
    case kAudioChannelLayoutTag_EAC3_7_1_H:
        layout = "\x01\x03\x02\x0A\x0B\x04\x0C\x0E"; break;
    case kAudioChannelLayoutTag_DTS_7_1:
        layout = "\x07\x03\x08\x01\x02\x0A\x0B\x04"; break;
    case kAudioChannelLayoutTag_WAVE_7_1:
        layout = "\x01\x02\x03\x04\x05\x06\x0A\x0B"; break;
    case kAudioChannelLayoutTag_Logic_7_1_B:
        layout = "\x01\x02\x0A\x0B\x05\x06\x03\x04"; break;
    /* 8ch */
    case kAudioChannelLayoutTag_Octagonal:
        layout = "\x01\x02\x05\x06\x03\x09\x0A\x0B"; break;
    case kAudioChannelLayoutTag_AAC_Octagonal:
        layout = "\x03\x01\x02\x0A\x0B\x05\x06\x09"; break;
    case kAudioChannelLayoutTag_DTS_8_0_A:
        layout = "\x07\x08\x01\x02\x0A\x0B\x05\x06"; break;
    case kAudioChannelLayoutTag_DTS_8_0_B:
        layout = "\x07\x03\x08\x01\x02\x0A\x09\x0B"; break;
    default:
        throw std::runtime_error("unsupported channel layout");
    }
    if (!layout)
        return;
    std::vector<char> channels(strlen(layout));
    std::copy(layout, layout + strlen(layout), channels.begin());
    parse_channels(d, channels);
}

void CAFFile::parse_channels(Format *d, const std::vector<char> &channels)
{
    for (auto it = channels.begin(); it != channels.end(); ++it)
        d->channel_mask |= 1 << (static_cast<uint8_t>(*it) - 1);
    if (channels.size() != d->asbd.mChannelsPerFrame
     || channels.size() != Helpers::bitcount(d->channel_mask))
        return;

    std::vector<const char*> v(channels.size());
    for (size_t i = 0; i < channels.size(); ++i)
        v[i] = channels.data() + i;
    std::sort(v.begin(), v.end(),
              [](const char *a, const char *b) -> bool { return *a < *b; });
    for (size_t i = 0; i < channels.size(); ++i)
        d->channel_map[i] = v[i] - &channels[0];
}

int64_t CAFFile::find_room_for_info(t_filesize *room_pos, t_filesize *info_pos,
                                    abort_callback &abort)
{
    uint32_t fcc, pre_fcc = 0;
    int64_t  size, size_acc = 0, max_size_acc = 0;
    t_filesize candidate_pos = 0, max_candidate_pos = 0;

    m_pfile->seek(8, abort);
    for (;;) {
        t_filesize pos = m_pfile->get_position(abort);
        if (pos >= m_pfile->get_size(abort))
            break;
        m_pfile->read_bendian_t(fcc,  abort);
        m_pfile->read_bendian_t(size, abort);
        switch (fcc) {
        case FOURCC('i','n','f','o'):
            *info_pos = pos;
            /* FALLTHROUGH */
        case FOURCC('f','r','e','e'):
            size_acc += size + 12;
            if (!candidate_pos) candidate_pos = pos;
            break;
        default:
            if (size_acc > max_size_acc) {
                max_size_acc      = size_acc;
                max_candidate_pos = candidate_pos;
            }
            size_acc      = 0;
            candidate_pos = 0;
        }
        m_pfile->skip(size, abort);
    }
    if (size_acc > max_size_acc) {
        max_size_acc      = size_acc;
        max_candidate_pos = candidate_pos;
    }
    *room_pos = max_candidate_pos;
    return max_size_acc - 12;
}
