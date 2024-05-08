#include "display.hpp"

#include "core/core.hpp"

#include <fstream>
#include <sstream>

screen_t::screen_t(uint32_t width, uint32_t height) : _width(width), _height(height) {
    _pixels.resize(_width * _height);
}

void screen_t::plot(uint32_t x, uint32_t y, pixel_t pixel) {
    _pixels[x * _width + y] = pixel;
}

void screen_t::to_disk(const std::filesystem::path& path) {
    std::stringstream s;
    s << "P3\n" << _width << ' ' << _height << "\n255\n";
    for (size_t j = 0; j < _height; j++) for (size_t i = 0; i < _width; i++) {
        pixel_t pixel = _pixels[i * _width + j];
        s << uint32_t(pixel.r) << ' ' << uint32_t(pixel.g) << ' ' << uint32_t(pixel.b) << '\n';
    }

    std::ofstream file{ path };
    check(file.is_open(), "failed to open file");
    file << s.str();
}
