#pragma once
#include "CAFDecoder.h"
#define NOMINMAX
#include "../SDK/foobar2000.h"

class FilePositionSaver
{
private:
    std::shared_ptr<IStreamReader> m_pstream;
    int64_t m_saved_position;
public:
    FilePositionSaver(std::shared_ptr<IStreamReader> &pstream)
        : m_pstream(pstream)
    {
        m_saved_position = m_pstream->get_position();
    }
    ~FilePositionSaver()
    {
        m_pstream->seek(m_saved_position, SEEK_SET);
    }
};

class CAFMetaData {
    std::shared_ptr<IStreamReader> m_pstream;
    typedef std::pair<std::string, std::string> kvpair_t;
    std::vector<kvpair_t> m_meta;
public:
    CAFMetaData(std::shared_ptr<IStreamReader> &pstream)
        : m_pstream(pstream)
    {
        FilePositionSaver saver(m_pstream);
        readInfoChunk();
    }
    void getInfo(file_info &pinfo);
private:
    uint64_t nextChunk(char *name);
    void readInfoChunk();
};

namespace meta_from_fb2k {
    void convertToInfoDictionary(const file_info &pinfo,
                                 CFDictionaryPtr *result);
}
