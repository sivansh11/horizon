#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "core/math.hpp"

// float32 depth + uint32_t slot_id
class visibility_pass_t {
public:
    visibility_pass_t(core::window_t& window, renderer::base_renderer_t& base_renderer) : window(window), base_renderer(base_renderer) {
        auto [width, height] = window.dimensions();

        std::tie(depth, depth_view) = gfx::helper::create_2D_image_and_image_view(base_renderer.context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        std::tie(visibility, visibility_view) = gfx::helper::create_2D_image_and_image_view(base_renderer.context, width, height, VK_FORMAT_R32_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        
        
    }


private:
    core::window_t& window;
    renderer::base_renderer_t& base_renderer;

    gfx::handle_image_t depth;
    gfx::handle_image_view_t depth_view;
    gfx::handle_image_t visibility;
    gfx::handle_image_view_t visibility_view;
    
};

#endif