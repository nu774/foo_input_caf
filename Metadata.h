#ifndef METADATA_H
#define METADATA_H

#include <string>
#include <vector>
#include <utility>
#include "../SDK/foobar2000.h"

namespace Metadata {
    typedef std::pair<std::string, std::string> string_pair;

    void get_entries(file_info *info, const std::vector<string_pair> &meta);
    void put_entries(std::vector<string_pair> *meta, const file_info &info);
}
#endif
