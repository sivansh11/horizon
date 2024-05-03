#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "utility.hpp"

int main() {
    core::window_t window{ "diffuse", 640, 420 };
    renderer::base_renderer_t renderer{ window };
    gfx::context_t& context = renderer.context;

    auto image = gfx::helper::load_image_from_path(context, renderer.command_pool, "../../assets/texture/noise.jpg", VK_FORMAT_R8G8B8A8_SRGB);
    
    gfx::config_image_view_t civ{};
    civ.handle_image = image;
    gfx::handle_image_view_t iv = context.create_image_view(civ);

    gfx::config_sampler_t cs{};
    gfx::handle_sampler_t s = context.create_sampler(cs);
    
    gfx::config_descriptor_set_layout_t cdsl{};
    cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t dsl = context.create_descriptor_set_layout(cdsl);

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_descriptor_set_layout(dsl);
    gfx::handle_pipeline_layout_t pl = context.create_pipeline_layout(cpl);

    gfx::config_pipeline_t cgp{};
    cgp.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    cgp.add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/swapchain/present_to_swapchain.vert").data(), .name = "swapchain vert", .type = gfx::shader_type_t::e_vertex}));
    cgp.add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/swapchain/present_to_swapchain.frag").data(), .name = "swapchain frag", .type = gfx::shader_type_t::e_fragment}));
    cgp.handle_pipeline_layout = pl;
    gfx::handle_pipeline_t gp = context.create_graphics_pipeline(cgp);

    auto ds = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_sparse, gfx::config_descriptor_set_t{.handle_descriptor_set_layout = dsl});
    renderer.update_descriptor_set(ds).push_image_write(0, renderer::image_descriptor_info_t{.handle_sampler = s, .handle_image_view = iv, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}).commit();

    while (!window.should_close()) {
        core::window_t::poll_events();

        renderer.begin();

        auto commandbuffer = renderer.current_commandbuffer();

        context.cmd_begin_rendering(commandbuffer, {renderer.swapchain_rendering_attachment({0, 0, 0, 0}, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)}, std::nullopt, VkRect2D{VkOffset2D{}, {640, 420}});
        context.cmd_bind_pipeline(commandbuffer, gp);
        context.cmd_bind_descriptor_sets(commandbuffer, gp, 0, { renderer.descriptor_set(ds) });
        auto [viewport, scissor] = fill_viewport_and_scissor_structs(640, 420);
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);
        context.cmd_end_rendering(commandbuffer);

        renderer.end();
    }

    return 0;
}