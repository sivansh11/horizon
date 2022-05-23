#include "horizon_utils.h"

namespace horizon
{
std::string read_file(const char *path, const FileType type)
{
    std::ifstream ifs;
    switch(type)
    {
        case FileType::TEXT:
            ifs = std::ifstream(path);
            break;
        case FileType::BINARY:
            ifs = std::ifstream(path, std::ios::binary);
            break;
    }
    runtime_assert(ifs.is_open(), "failed to open file");
    return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}

void split_string(const std::string& str, const char delimiter, std::vector<std::string> &out)
{
    std::vector<std::string> &strings=out;
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos)
    {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }
    strings.push_back(str.substr(prev));
}

void save_string(std::string &str, const char *path)
{
    std::ofstream out(path);
    out << str;
    out.close();
}


} // namespace horizon
