#include "horizon/core/core.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/model.hpp"
#include "horizon/core/window.hpp"

#include "camera.hpp"
#include "renderer.hpp"

#include <GLFW/glfw3.h>
#include <cassert>
#include <imgui.h>
#include <string>

int main(int argc, char **argv) {
  check(argc == 4, "app [width] [height] [validations]");

  core::log_t::set_log_level(core::log_level_t::e_info);

  core::window_t window{"app", (uint32_t)std::stoi(argv[1]),
                        (uint32_t)std::stoi(argv[2])};

  renderer_t renderer{window, (bool)std::stoi(argv[3])};

  ecs::scene_t scene{100000};

  {
    auto id = scene.create();
    scene.construct<core::raw_model_t>(id) = core::load_model_from_path(
        "../../../horizon_cpy/assets/models/teapot.obj");
  }

  float target_fps = 10000000.f;
  auto last_time = std::chrono::system_clock::now();
  core::frame_timer_t frame_timer{60.f};

  camera_t camera{window};

  while (!window.should_close()) {
    core::clear_frame_function_times();
    core::window_t::poll_events();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
      break;

    auto current_time = std::chrono::system_clock::now();
    auto time_difference = current_time - last_time;
    if (time_difference.count() / 1e6 < 1000.f / target_fps) {
      continue;
    }
    last_time = current_time;
    auto dt = frame_timer.update();

    camera.update(dt.count());

    renderer.begin(scene, camera);

    ImGui::Begin("Test");
    ImGui::End();

    renderer.end();
  }

  return 0;
}
