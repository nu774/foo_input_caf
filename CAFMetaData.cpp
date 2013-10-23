#include "CAFMetaData.h"
#include "strutil.h"
#include "util.h"
#include "dl.h"

/*
 * Apple Core Audio Format Specification 1.0 defines some keys
 * for info chunk metadata, and keys that are all lowercase are 
 * reserved by Apple (even if not defined by the spec).
 * Therefore, we use uppercase keys for tags that don't apear in the spec.
 */
namespace meta_to_fb2k {
    void set_to_info(file_info &pinfo, const char *key, const char *value)
    {
        pinfo.info_set(key, value);
    }
    void disc_number(file_info &pinfo, const char *key, const char *value)
    {
        strutil::Tokenizer<char> tokens(value, "/");
        char *num = tokens.next();
        char *total = tokens.rest();
        pinfo.meta_set("discnumber", num);
        if (total)
            pinfo.meta_set("totaldiscs", total);
    }
    void replaygain(file_info &pinfo, const char *key, const char *value)
    {
        pinfo.info_set_replaygain(key, value);
    }
    void comments(file_info &pinfo, const char *key, const char *value)
    {
        pinfo.meta_set("comment", value);
    }
    void encoding_application(file_info &pinfo, const char *key,
                              const char *value)
    {
        pinfo.info_set("tool", value);
    }
    void lyricist(file_info &pinfo, const char *key, const char *value)
    {
        pinfo.meta_set("writer", value);
    }
    void recorded_date(file_info &pinfo, const char *key, const char *value)
    {
        pinfo.meta_set("record date", value);
    }
    void tempo(file_info &pinfo, const char *key, const char *value)
    {
        pinfo.meta_set("bpm", value);
    }
    void track_number(file_info &pinfo, const char *key, const char *value)
    {
        strutil::Tokenizer<char> tokens(value, "/");
        char *num = tokens.next();
        char *total = tokens.rest();
        pinfo.meta_set("tracknumber", num);
        if (total)
            pinfo.meta_set("totaltracks", total);
    }
    void year(file_info &pinfo, const char *key, const char *value)
    {
        pinfo.meta_set("date", value);
    }

    struct meta_handler_entry_t {
        const char *key;
        void (*handler)(file_info &, const char *, const char *);
    } handlers[] = {
        { "approximate duration in seconds",    set_to_info             },
        { "channel layout",                     set_to_info             },
        { "comments",                           comments                },
        { "DISC",                               disc_number             },
        { "DISC NUMBER",                        disc_number             },
        { "ENCODER",                            encoding_application    },
        { "encoding application",               encoding_application    },
        { "lyricist",                           lyricist                },
        { "nominal bit rate",                   set_to_info             },
        { "recorded date",                      recorded_date           },
        { "REPLAYGAIN_ALBUM_GAIN",              replaygain              },
        { "REPLAYGAIN_ALBUM_PEAK",              replaygain              },
        { "REPLAYGAIN_TRACK_GAIN",              replaygain              },
        { "REPLAYGAIN_TRACK_PEAK",              replaygain              },
        { "source bit depth",                   set_to_info             },
        { "source encoder",                     set_to_info             },
        { "tempo",                              tempo                   },
        { "TRACK",                              track_number            },
        { "track number",                       track_number            },
        { "year",                               year                    },
    };
    void process_entry(file_info &pinfo, const char *key, const char *value)
    {
        struct comparator {
            static int call(const void *k, const void *v)
            {
                const char *key = static_cast<const char *>(k);
                const meta_handler_entry_t *ent
                    = static_cast<const meta_handler_entry_t *>(v);
                return strcasecmp(key, ent->key);
            }
        };
        const meta_handler_entry_t *entry =
            static_cast<const meta_handler_entry_t *>(
                std::bsearch(key, handlers, util::sizeof_array(handlers),
                             sizeof(handlers[0]), comparator::call));
        if (entry)
            entry->handler(pinfo, key, value);
        else
            pinfo.meta_set(key, value);
    }
}

namespace meta_from_fb2k {
    const char * known_keys[][2] = {
        { "ALBUM",                      "album"                         },
        { "ARTIST",                     "artist"                        },
        { "BPM",                        "tempo"                         },
        { "COMMENT",                    "comments"                      },
        { "COMPOSER",                   "composer"                      },
        { "COPYRIGHT",                  "copyright"                     },
        { "DATE",                       "year"                          },
        { "GENRE",                      "genre"                         },
        { "KEY SIGNATURE",              "key signature"                 },
        { "LYRICIST",                   "lyricist"                      },
        { "RECORD DATE",                "recorded date"                 },
        { "SUBTITLE",                   "subtitle"                      },
        { "TEMPO",                      "tempo"                         },
        { "TIME SIGNATURE",             "time signature"                },
        { "TITLE",                      "title"                         },
        { "WRITER",                     "lyricist"                      },
        { "approximate duration in seconds",
                                        "approximate duration in seconds" },
        { "channel layout",             "channel layout"                },
        { "nominal bitrate",            "nominal bitrate"               },
        { "source bit depth",           "source bit depth"              },
        { "source encoder",             "source encoder"                },
        { "tool",                       "encoding application"          },
    };

    class CFString8 {
    public:
        CFString8(const std::string &s)
            : m_value(cautil::W2CF(strutil::us2w(s)))
        {}
        operator CFStringRef() { return m_value.get(); }
        operator const void*() { return m_value.get(); }
    private:
        CFStringPtr m_value;
    };

    /*
     * merge pair like tracknumber/totaltracks into a single key-value,
     * and remove original entities from "info" (for succeeding tasks)
     */
    void number_pair(file_info &info, CFMutableDictionaryRef dict,
                     const char *key)
    {
        char number_key[32], total_key[32], caf_key[32];
        std::sprintf(number_key, "%sNUMBER", key);
        std::sprintf(total_key,  "TOTAL%sS", key);
        std::sprintf(caf_key, "%s number", key);

        if (!std::strcmp(key, "TRACK"))
            _strlwr(caf_key);
        else
            _strupr(caf_key);

        const char *number = info.meta_get(number_key, 0);
        const char *total  = info.meta_get(total_key,  0);
        if (number) {
            std::stringstream ss;
            ss << number;
            if (total) ss << "/" << total;
            CFDictionarySetValue(dict, CFString8(caf_key), CFString8(ss.str()));
        }
        if (number) info.meta_remove_field(number_key);
        if (total)  info.meta_remove_field(total_key);
    }

    void put_entry(CFMutableDictionaryRef dict, const char *key,
                   const char *value, bool only_found)
    {
        typedef const char *entry_t[2];
        struct comparator {
            static int call(const void *k, const void *v)
            {
                const char *key = static_cast<const char *>(k);
                const entry_t *ent = static_cast<const entry_t *>(v);
                return std::strcmp(key, (*ent)[0]);
            }
        };
        entry_t *entry = static_cast<entry_t*>(
            std::bsearch(key, known_keys, util::sizeof_array(known_keys),
                         sizeof(known_keys[0]), comparator::call));

        if (!entry && only_found)
            return;
        std::string skey;
        if (entry)
            skey = (*entry)[1];
        else
            std::transform(key, key + strlen(key),
                           std::back_inserter(skey),
                           toupper);
        CFDictionarySetValue(dict, CFString8(skey), CFString8(value));
    }
    void put_replaygain(const file_info &info, __CFDictionary *dict)
    {
        replaygain_info rg = info.get_replaygain();
        char buf[replaygain_info::text_buffer_size];

        if (rg.is_album_gain_present()) {
            rg.format_album_gain(buf);
            put_entry(dict, "REPLAYGAIN_ALBUM_GAIN", buf, false);
        }
        if (rg.is_track_gain_present()) {
            rg.format_track_gain(buf);
            put_entry(dict, "REPLAYGAIN_TRACK_GAIN", buf, false);
        }
        if (rg.is_album_peak_present()) {
            rg.format_album_peak(buf);
            put_entry(dict, "REPLAYGAIN_ALBUM_PEAK", buf, false);
        }
        if (rg.is_track_peak_present()) {
            rg.format_track_peak(buf);
            put_entry(dict, "REPLAYGAIN_TRACK_PEAK", buf, false);
        }
    }
    void convertToInfoDictionary(const file_info &pinfo,
                                 CFDictionaryPtr *result)
    {
        file_info_impl info(pinfo); // get a copy and work on it
        std::shared_ptr<const __CFDictionary>
            dic(cautil::CreateDictionary(0), CFRelease);
        __CFDictionary *dp = const_cast<__CFDictionary*>(dic.get());

        number_pair(info, dp, "TRACK");
        number_pair(info, dp, "DISC");

        t_size count = info.meta_get_count();
        // XXX only preserve first value of each tag
        for (t_size i = 0; i < count; ++i)
            put_entry(dp, info.meta_enum_name(i),
                      info.meta_enum_value(i, 0), false);

        count = info.info_get_count();
        for (t_size i = 0; i < count; ++i)
            put_entry(dp, info.info_enum_name(i),
                      info.info_enum_value(i), true);

        put_replaygain(info, dp);
        result->swap(dic);
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
