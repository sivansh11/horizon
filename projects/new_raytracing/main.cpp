#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"
// #include "display.hpp"
// #include "bvh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>



int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "this_app", 800, 800 };
    gfx::context_t context{ true };

    gfx::handle_shader_t shader = context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/test/hello-world.slang").data(), .name="test", .type = gfx::shader_type_t::e_compute, .language = gfx::shader_language_t::e_glsl });

    context.wait_idle();

    return 0;
}