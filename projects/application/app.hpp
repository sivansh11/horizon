#ifndef APP_HPP
#define APP_HPP

#include "core/core.hpp"
#include "core/window.hpp"
#include "core/ecs.hpp"
#include "core/event.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "event_types.hpp"
#include "panel.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

class app_t {
public:
    app_t();
    ~app_t();

    void run();

private:
    core::ref<core::window_t> window;
    core::ref<gfx::context_t> context;

    core::dispatcher_t dispatcher{};

    gfx::handle_sampler_t gloabl_sampler;
    gfx::handle_image_t final_image;
    gfx::handle_image_view_t final_image_view;
    core::ref<renderer::base_renderer_t> base_renderer;
};

#endif