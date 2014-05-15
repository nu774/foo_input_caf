#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <iterator>
#include "Metadata.h"

namespace {
    void set_to_info(file_info *info, const char *key, const char *value)
    {
        info->info_set(key, value);
    }
    void disc_number(file_info *info, const char *key, const char *value)
    {
        std::stringstream ss(value);
        std::string tok;
        std::getline(ss, tok, '/');
        info->meta_set("discnumber", tok.c_str());
        if (std::getline(ss, tok, '\0'))
            info->meta_set("totaldiscs", tok.c_str());
    }
    void replaygain(file_info *info, const char *key, const char *value)
    {
        info->info_set_replaygain(key, value);
    }
    void comments(file_info *info, const char *key, const char *value)
    {
        info->meta_add("comment", value);
    }
    void encoding_application(file_info *info, const char *key,
                              const char *value)
    {
        info->info_set("tool", value);
    }
    void lyricist(file_info *info, const char *key, const char *value)
    {
        info->meta_add("writer", value);
    }
    void recorded_date(file_info *info, const char *key, const char *value)
    {
        info->meta_add("date", value);
    }
    void tempo(file_info *info, const char *key, const char *value)
    {
        info->meta_add("bpm", value);
    }
    void track_number(file_info *info, const char *key, const char *value)
    {
        std::stringstream ss(value);
        std::string tok;
        std::getline(ss, tok, '/');
        info->meta_set("tracknumber", tok.c_str());
        if (std::getline(ss, tok, '\0'))
            info->meta_set("totaltracks", tok.c_str());
    }
    void year(file_info *info, const char *key, const char *value)
    {
        info->meta_add("date", value);
    }

    struct meta_handler_entry_t {
        const char *key;
        void (*handler)(file_info *, const char *, const char *);
    } handlers[] = {
        { "approximate duration in seconds",    set_to_info             },
        { "channel layout",                     set_to_info             },
        { "comments",                           comments                },
        { "DISC NUMBER",                        disc_number             },
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
        { "track number",                       track_number            },
        { "year",                               year                    },
    };

    void get_entry(file_info *info, const char *key, const char *value)
    {
        typedef meta_handler_entry_t ent_t;
        struct Lambda {
            static int op(const void *k, const void *v) {
                auto key = static_cast<const char *>(k);
                auto ent = static_cast<const ent_t*>(v);
                return _stricmp(key, ent->key);
            }
        };
        auto entry =
            static_cast<ent_t *>(std::bsearch(key, handlers,
                                              pfc::array_size_t(handlers),
                                              sizeof(handlers[0]), Lambda::op));
        if (entry)
            entry->handler(info, key, value);
        else
            info->meta_add(key, value);
    }

    const char *known_keys[][2] = {
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

    typedef std::pair<std::string, std::string> string_pair;

    string_pair make_string_pair(const char *first, const char *second)
    {
        return std::make_pair(std::string(first), std::string(second));
    }
    void put_entry(std::vector<string_pair> *meta, const char *key,
                   const char *value, bool only_found=false)
    {
        typedef const char *entry_t[2];
        struct Lambda {
            static int op(const void *k, const void *v) {
                auto key = static_cast<const char *>(k);
                auto ent = static_cast<const entry_t *>(v);
                return _stricmp(key, (*ent)[0]);
            }
        };
        entry_t *entry =
            static_cast<entry_t*>(std::bsearch(key, known_keys,
                                               pfc::array_size_t(known_keys),
                                               sizeof(known_keys[0]),
                                               Lambda::op));
        if (!entry && only_found)
            return;
        std::string skey;
        if (entry)
            skey = (*entry)[1];
        else
            std::transform(key, key + strlen(key), std::back_inserter(skey),
                           toupper);
        meta->push_back(make_string_pair(skey.c_str(), value));
    }
    void put_rg(std::vector<string_pair> *meta, const file_info &info)
    {
        replaygain_info rg = info.get_replaygain();
        char buf[replaygain_info::text_buffer_size];

        if (rg.is_album_gain_present()) {
            rg.format_album_gain(buf);
            meta->push_back(make_string_pair("REPLAYGAIN_ALBUM_GAIN", buf));
        }
        if (rg.is_track_gain_present()) {
            rg.format_track_gain(buf);
            meta->push_back(make_string_pair("REPLAYGAIN_TRACK_GAIN", buf));
        }
        if (rg.is_album_peak_present()) {
            rg.format_album_peak(buf);
            meta->push_back(make_string_pair("REPLAYGAIN_ALBUM_PEAK", buf));
        }
        if (rg.is_track_peak_present()) {
            rg.format_track_peak(buf);
            meta->push_back(make_string_pair("REPLAYGAIN_TRACK_PEAK", buf));
        }
    }
}

namespace Metadata {
    void get_entries(file_info *info, const std::vector<string_pair> &meta)
    {
        for (size_t i = 0; i < meta.size(); ++i)
            get_entry(info, meta[i].first.c_str(), meta[i].second.c_str());
    }

    void put_entries(std::vector<string_pair> *tags, const file_info &info)
    {
        unsigned track = 0, track_total = 0, disc = 0, disc_total = 0;
        std::vector<string_pair> meta;
        t_size count = info.meta_get_count();
        for (t_size i = 0; i < count; ++i) {
            t_size vcount = info.meta_enum_value_count(i);
            const char *name = info.meta_enum_name(i);
            const char *v = info.meta_enum_value(i, 0);
            if (!_stricmp(name, "TRACKNUMBER"))
                std::sscanf(v, "%u/%u", &track, &track_total);
            else if (!_stricmp(name, "DISCNUMBER"))
                std::sscanf(v, "%u/%u", &disc, &disc_total);
            else if (!_stricmp(name, "TOTALTRACKS"))
                std::sscanf(v, "%u", &track_total);
            else if (!_stricmp(name, "TOTALDISCS"))
                std::sscanf(v, "%u", &disc_total);
            else
                for (t_size j = 0; j < vcount; ++j)
                    put_entry(&meta, name, info.meta_enum_value(i, j));
        }
        put_rg(&meta, info);

        auto put_number_pair =
            [&meta](const char *name, unsigned number, unsigned total) -> void
        {
            if (number) {
                char buf[256];
                if (total) sprintf(buf, "%u/%u", number, total);
                else       sprintf(buf, "%u", number);
                meta.push_back(make_string_pair(name, buf));
            }
        };
        put_number_pair("track number", track, track_total);
        put_number_pair("DISC NUMBER", disc, disc_total);

        count = info.info_get_count();
        for (t_size i = 0; i < count; ++i)
            put_entry(&meta, info.info_enum_name(i), info.info_enum_value(i),
                      true);
        tags->swap(meta);
    }
}
