#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <filesystem>
#include <fstream>

#include "horizon/core/math.hpp"

struct image_t {
  image_t(uint32_t width, uint32_t height) : width(width), height(height) {
    p_pixels = new core::vec4[width * height];
  }

  ~image_t() { delete[] p_pixels; }

  core::vec4 &at(uint32_t x, uint32_t y) { return p_pixels[y * width + x]; }

  void to_disk(const std::filesystem::path &path) {
    std::stringstream s;
    s << "P3\n" << width << ' ' << height << "\n255\n";
    for (int32_t j = height - 1; j >= 0; j--)
      for (int32_t i = 0; i < width; i++) {
        core::vec4 pixel = at(i, j);
        s << uint32_t(clamp(pixel.r, 0.f, 1.f) * 255) << ' '
          << uint32_t(clamp(pixel.g, 0.f, 1.f) * 255) << ' '
          << uint32_t(clamp(pixel.b, 0.f, 1.f) * 255) << '\n';
      }

    std::ofstream file{path};
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file");
    }
    file << s.str();
    file.close();
  }

  template <typename T> T clamp(T val, T min, T max) {
    return val > max ? max : val < min ? min : val;
  }

  uint32_t width, height;
  core::vec4 *p_pixels;
};

#endif
