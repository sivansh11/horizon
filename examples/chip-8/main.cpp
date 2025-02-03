#include "GLFW/glfw3.h"
#include "horizon/core/logger.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vulkan/vulkan_core.h>

#include "chip-8.hpp"
#include "renderer.hpp"

std::string read_file(const std::filesystem::path &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("Failed to open file");
  size_t file_size = static_cast<size_t>(file.tellg());
  std::string buffer;
  buffer.reserve(file_size);
  file.seekg(0);
  size_t counter = 0;
  while (counter++ != file_size)
    buffer += file.get();
  file.close();
  return buffer;
}

int main(int argc, char **argv) {
  core::log_t::set_log_level(core::log_level_t::e_info);

  if (argc == 1) {
    // interpreted mode
  } else if (argc == 2 || argc == 3) {
    // file mode
    auto chip8 = chip8_t<64, 32>::create();
    chip8.upload_font(font, sizeof(font));
    auto obj = read_file(argv[1]);
    chip8.upload_rom((uint8_t *)obj.data(), obj.size());

    core::window_t window{"chip-8", 1200, 800};

    renderer_t renderer{window, true};

    renderer.is_step = false;
    if (argc == 3)
      renderer.is_step = std::stoi(argv[2]);

    bool exit = false;

    auto tick = [&]() {
      float tick_target_fps = 60.f;
      auto tick_last_time = std::chrono::system_clock::now();

      while (!window.should_close()) {
        if (exit)
          break;
        auto tick_current_time = std::chrono::system_clock::now();
        auto tick_time_difference = tick_current_time - tick_last_time;
        if (tick_time_difference.count() / 1e6 < 1000.f / tick_target_fps) {
          continue;
        }
        tick_last_time = tick_current_time;
        chip8.tick();
      }
    };

    std::thread thread{tick};

    float target_fps = 1000000.f;
    auto last_time = std::chrono::system_clock::now();

    instruction_t inst;
    while (!window.should_close()) {
      core::window_t::poll_events();
      auto current_time = std::chrono::system_clock::now();
      auto time_difference = current_time - last_time;
      if (time_difference.count() / 1e6 < 1000.f / target_fps) {
        continue;
      }
      last_time = current_time;

      if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
        exit = true;
        break;
      }

      if (glfwGetKey(window, GLFW_KEY_1))
        chip8.press_key(1);
      else
        chip8.release_key(1);

      if (glfwGetKey(window, GLFW_KEY_2))
        chip8.press_key(2);
      else
        chip8.release_key(2);

      if (glfwGetKey(window, GLFW_KEY_3))
        chip8.press_key(3);
      else
        chip8.release_key(3);

      if (glfwGetKey(window, GLFW_KEY_4))
        chip8.press_key(C);
      else
        chip8.release_key(C);

      if (glfwGetKey(window, GLFW_KEY_Q))
        chip8.press_key(4);
      else
        chip8.release_key(4);

      if (glfwGetKey(window, GLFW_KEY_W))
        chip8.press_key(5);
      else
        chip8.release_key(5);

      if (glfwGetKey(window, GLFW_KEY_E))
        chip8.press_key(6);
      else
        chip8.release_key(6);

      if (glfwGetKey(window, GLFW_KEY_R))
        chip8.press_key(D);
      else
        chip8.release_key(D);

      if (glfwGetKey(window, GLFW_KEY_A))
        chip8.press_key(7);
      else
        chip8.release_key(7);

      if (glfwGetKey(window, GLFW_KEY_S))
        chip8.press_key(8);
      else
        chip8.release_key(8);

      if (glfwGetKey(window, GLFW_KEY_D))
        chip8.press_key(9);
      else
        chip8.release_key(9);

      if (glfwGetKey(window, GLFW_KEY_F))
        chip8.press_key(E);
      else
        chip8.release_key(E);

      if (glfwGetKey(window, GLFW_KEY_Z))
        chip8.press_key(A);
      else
        chip8.release_key(A);

      if (glfwGetKey(window, GLFW_KEY_X))
        chip8.press_key(0);
      else
        chip8.release_key(0);

      if (glfwGetKey(window, GLFW_KEY_C))
        chip8.press_key(B);
      else
        chip8.release_key(B);

      if (glfwGetKey(window, GLFW_KEY_V))
        chip8.press_key(F);
      else
        chip8.release_key(F);

      if (renderer.is_step) {
        static bool pressed = false;
        if (glfwGetKey(window, GLFW_KEY_SPACE)) {
          if (!pressed) {
            inst = chip8.fetch();
            chip8.decode_and_execute();
            pressed = true;
          }
        } else {
          pressed = false;
        }
      } else {
        inst = chip8.fetch();
        chip8.decode_and_execute();
      }

      renderer.render(chip8);
    }
    thread.join();


  } else {
    throw std::runtime_error("chip-8 [optional: object file] [step]");
  }

  return 0;
}
