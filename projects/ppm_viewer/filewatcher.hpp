#ifndef FILE_WATCHER_HPP
#define FILE_WATCHER_HPP

#include <filesystem>

class filewatcher_t {
public:
    filewatcher_t(const std::filesystem::path& filepath) : _filepath(filepath) {
        _last_write = std::filesystem::last_write_time(_filepath);
    }

    bool has_changed() {
        auto new_write = std::filesystem::last_write_time(_filepath);
        if (new_write != _last_write) {
            _last_write = new_write;
            return true;
        }
        return false;
    }

private:
    const std::filesystem::path _filepath;
    std::filesystem::file_time_type _last_write;
};

#endif