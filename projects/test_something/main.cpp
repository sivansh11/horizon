#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);

    return 0;
}