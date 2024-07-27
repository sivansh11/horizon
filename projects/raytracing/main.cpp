#include "core/core.hpp"
#include "core/window.hpp"
#include "core/bvh.hpp"
#include "core/random.hpp"
#include "core/ecs.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/bindless_manager.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


int main() {
    core::window_t window{ "bindless manager test", 640, 640 };
    gfx::context_t context{ true };
    auto [width, height] = window.dimensions();
    auto [final, final_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto sampler = context.create_sampler({});
    renderer::base_renderer_t renderer{ window, context, sampler, final_view };
    gfx::bindless_manager_t bindless_manager{ context };

    core::scene_t scene{};
    {
        auto id = scene.create();
        scene.construct<core::model_t>(id) = core::load_model_from_path("../../assets/models/corenl_box.obj");
    }


    return 0;
}