#include "CAFMetaData.h"
#include "strutil.h"
#include "util.h"

struct meta_handler_entry_t {
    const char *key;
    void (*handler)(file_info &, const char *);
};

namespace meta_to_fb2k {
    void comments(file_info &pinfo, const char *value)
    {
	pinfo.meta_set("comment", value);
    }
    void encoding_application(file_info &pinfo, const char *value)
    {
	pinfo.info_set("tool", value);
    }
    void recorded_date(file_info &pinfo, const char *value)
    {
	pinfo.meta_set("record date", value);
    }
    void track_number(file_info &pinfo, const char *value)
    {
	std::vector<char> buf(std::strlen(value) + 1);
	std::strcpy(&buf[0], value);
	char *num, *total = &buf[0];
	num = strutil::strsep(&total, "/");
	pinfo.meta_set("tracknumber", num);
	if (total && *total)
	    pinfo.meta_set("totaltracks", total);
    }
    void year(file_info &pinfo, const char *value)
    {
	pinfo.meta_set("date", value);
    }
    const meta_handler_entry_t handlers[] = {
	{ "comments",			comments		},
	{ "encoding application",	encoding_application	},
	{ "recorded date",		recorded_date		},
	{ "track number",		track_number		},
	{ "year",			year			},
	{ 0,				0			}
    };
    void process_entry(file_info &pinfo, const char *key, const char *value)
    {
	const meta_handler_entry_t *entry = handlers;
	for (; entry->key; ++entry) {
	    if (!std::strcmp(key, entry->key)) {
		entry->handler(pinfo, value);
		return;
	    }
	}
	pinfo.meta_set(key, value);
    }
}

void CAFMetaData::getInfo(file_info &pinfo)
{
    std::vector<kvpair_t>::const_iterator it;
    for (it = m_meta.begin(); it != m_meta.end(); ++it)
	meta_to_fb2k::process_entry(pinfo, it->first.c_str(),
				    it->second.c_str());
}

uint64_t CAFMetaData::nextChunk(char *name)
{
    uint64_t size;
    if (m_pstream->read(name, 4) != 4)
	return 0;
    if (m_pstream->read(&size, 8) != 8)
	return 0;
    return util::b2host64(size);
}

void CAFMetaData::readInfoChunk()
{
    if (m_pstream->seek(8, SEEK_SET) < 0)
	return;
    uint64_t size;
    char chunk_name[4];
    while ((size = nextChunk(chunk_name)) > 0) {
	if (!std::memcmp(chunk_name, "info", 4))
	    break;
	if (m_pstream->seek(size, SEEK_CUR) < 0)
	    return;
    }
    if (size <= 4)
	return;

    uint32_t num_entries;
    if (m_pstream->read(&num_entries, 4) != 4)
	return;
    num_entries = util::b2host32(num_entries);
    size -= 4;
    std::vector<char> buffer(size);
    if ((size = m_pstream->read(&buffer[0], size)) == 0)
	return;
    char *bp = &buffer[0], *endp = bp + size;
    std::vector<std::string> tokens;
    do {
	tokens.push_back(bp);
	bp += tokens.back().size() + 1;
    } while (bp < endp);
    num_entries = std::min(static_cast<size_t>(num_entries),
			   tokens.size() >> 1); 
    for (uint32_t i = 0; i < num_entries; ++i)
	m_meta.push_back(std::make_pair(tokens[2 * i], tokens[2 * i + 1]));
}
