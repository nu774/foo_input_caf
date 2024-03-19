#ifndef CAFFILE_H
#define CAFFILE_H

#include <cstdint>
#include <vector>
#include <string>
#include <utility>
#include "../SDK/foobar2000-winver.h"
#include "../SDK/foobar2000.h"
#include "CoreAudio/CoreAudioTypes.h"
#include "Helpers.h"
#include "Metadata.h"

class CAFFile {
public:
    struct Format {
        AudioStreamBasicDescription  asbd;
        uint32_t                     channel_mask;
        std::vector<char>            channel_map;

        Format(): channel_mask(0) { std::memset(&asbd, 0, sizeof asbd); }
    };
private:
    service_ptr_t<file>                               m_pfile;
    std::vector<std::pair<std::string, std::string> > m_tags;
    Format                                            m_primary_format;
    std::vector<Format>                               m_layered_formats;
    std::vector<uint8_t>                              m_magic_cookie;
    AudioFilePacketTableInfo                          m_packet_info;
    std::vector<AudioStreamPacketDescription>         m_packet_table;
    t_filesize                                        m_data_offset;
    t_filesize                                        m_data_size;
    bool                                              m_nearly_cbr;
    int64_t                                           m_duration;
public:
    CAFFile(const service_ptr_t<file> &file, abort_callback &abort)
        : m_pfile(file), m_data_offset(0), m_data_size(0),
          m_nearly_cbr(true), m_duration(0)
    {
        memset(&m_packet_info, 0, sizeof m_packet_info);
        parse(abort);
    }
    const Format &format() const
    {
        return m_layered_formats.size() ? m_layered_formats[0]
                                        : m_primary_format;
    }
    const Format &primary_format() const
    {
        return m_primary_format;
    }
    int64_t duration() const /* in number of PCM frames */
    {
        return m_duration;
    }
    int64_t num_packets() const
    {
        if (m_packet_table.size())
            return static_cast<uint32_t>(m_packet_table.size());
        else
            return m_data_size / format().asbd.mBytesPerPacket;
    }
    int32_t start_offset() const
    {
        return m_packet_info.mPrimingFrames * tscale() + .5;
    }
    uint32_t end_padding() const
    {
        return m_packet_info.mRemainderFrames * tscale() + .5;
    }
    bool is_cbr() const
    { 
        return m_nearly_cbr;
    }
    uint32_t bitrate() const /* in kbps */
    {
        double bps = m_data_size * format().asbd.mSampleRate / m_duration * 8;
        return static_cast<uint32_t>(bps / 1000 + 0.5);
    }
    void get_magic_cookie(std::vector<uint8_t> *data) const;
    
    /* returns position in bytes, optionally fills packet size */
    int64_t packet_info(int64_t index, uint32_t *size=0) const;

    uint32_t read_packets(int64_t offset, uint32_t count,
                          std::vector<uint8_t> *data, abort_callback &abort);

    void get_metadata(file_info &info)
    {
        Metadata::get_entries(&info, m_tags);
    }
    void set_metadata(const file_info &info, abort_callback &abort);
    /*
     * update format with analyzed information from the decoder.
     */
    void update_format(const AudioStreamBasicDescription &asbd)
    {
        Format format;
        format.asbd = asbd;
        m_layered_formats.insert(m_layered_formats.begin(), format);
        calc_duration();
    }
private:
    CAFFile(const CAFFile &);
    CAFFile& operator=(const CAFFile &);

    double tscale() const
    {
        if (!m_layered_formats.size())
            return 1.0;
        else
            return format().asbd.mSampleRate
                    / m_primary_format.asbd.mSampleRate;
    }
    void parse(abort_callback &abort);
    void parse_desc(Format *d,    abort_callback &abort);
    void parse_chan(Format *d,    abort_callback &abort);
    void parse_ldsc(int64_t size, abort_callback &abort);
    void parse_kuki(int64_t size, abort_callback &abort);
    void parse_info(int64_t size, abort_callback &abort);
    void parse_pakt(int64_t size, abort_callback &abort);
    void calc_duration();
    void parse_channel_layout_tag(Format *d, uint32_t tag);
    void parse_channels(Format *d, const std::vector<char> &channels);
    int64_t find_room_for_info(t_filesize *room_pos, t_filesize *info_pos,
                               abort_callback &abort);
};

#endif
