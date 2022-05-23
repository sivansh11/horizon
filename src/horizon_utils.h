#ifndef GENERAL_UTILS_H
#define GENERAL_UTILS_H

#include "horizon_core.h"

namespace horizon
{
enum FileType
{
    TEXT,
    BINARY
};
std::string read_file(const char *path, const FileType type=FileType::TEXT);
void split_string(const std::string& str, const char delimiter, std::vector<std::string> &out);
void save_string(std::string &str, const char *path);
} // namespace horizon

#endif