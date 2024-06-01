#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <filesystem>
#include <vector>
#include <cstdint>

struct pixel_t {
    uint8_t r, g, b, a;
};

struct screen_t {
    screen_t(uint32_t width, uint32_t height);

    void plot(uint32_t x, uint32_t y, pixel_t pixel);

    void to_disk(const std::filesystem::path& path);

    uint32_t _width, _height;
    std::vector<pixel_t> _pixels;
};

#endif