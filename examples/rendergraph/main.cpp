#include "horizon/core/logger.hpp"
#include "imgui.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"

int main() {
  core::ref<core::window_t> window =
      core::make_ref<core::window_t>("rendergraph", 640, 420);
  core::ref<gfx::context_t> context = core::make_ref<gfx::context_t>(true);
  core::ref<gfx::base_t>    base = core::make_ref<gfx::base_t>(window, context);

  gfx::helper::imgui_init(
      *window, *context, base->_swapchain,
      context
          ->get_image(context->get_swapchain(base->_swapchain).handle_images[0])
          .config.vk_format);

  // this is an external resource
  gfx::handle_image_t random = gfx::helper::load_image_from_path_instant(
      *context, base->_command_pool, "./examples/rendergraph/assets/noise.jpg",
      VK_FORMAT_R8G8B8A8_SRGB);
  gfx::handle_image_view_t random_view =
      context->create_image_view({.handle_image = random});

  gfx::config_descriptor_set_layout_t cdsl{};
  cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                          VK_SHADER_STAGE_ALL);
  cdsl.add_layout_binding(1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_ALL);
  gfx::handle_descriptor_set_layout_t dsl =
      context->create_descriptor_set_layout(cdsl);
  gfx::config_pipeline_layout_t cpl{};
  cpl.add_descriptor_set_layout(dsl);
  gfx::handle_pipeline_layout_t pl = context->create_pipeline_layout(cpl);
  gfx::config_pipeline_t        cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_color_attachment(
      context
          ->get_image(context->get_swapchain(base->_swapchain).handle_images[0])
          .config.vk_format,
      gfx::default_color_blend_attachment());
  cp.add_shader(gfx::helper::create_slang_shader(
      *context, "examples/rendergraph/shaders/example.slang",
      gfx::shader_type_t::e_vertex));
  cp.add_shader(gfx::helper::create_slang_shader(
      *context, "examples/rendergraph/shaders/example.slang",
      gfx::shader_type_t::e_fragment));
  gfx::handle_pipeline_t p = context->create_graphics_pipeline(cp);

  gfx::handle_descriptor_set_t ds =
      context->allocate_descriptor_set({.handle_descriptor_set_layout = dsl});

  gfx::handle_sampler_t sampler = context->create_sampler({});

  context->update_descriptor_set(ds)
      .push_image_write(
          0,
          gfx::image_descriptor_info_t{
              .handle_image_view = random_view,
              .vk_image_layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
      .push_image_write(1, {.handle_sampler = sampler})
      .commit();

  while (!window->should_close()) {
    core::window_t::poll_events();
    if (window->get_key_pressed(core::key_t::e_escape)) break;
    if (window->get_key_pressed(core::key_t::e_q)) break;

    base->begin();

    gfx::handle_commandbuffer_t cmd = base->current_commandbuffer();

    gfx::rendergraph_t rendergraph{};
    rendergraph
        .add_pass([&](gfx::handle_commandbuffer_t cmd) {
          auto [width, height] = window->dimensions();
          VkRect2D vk_rect2d{};
          vk_rect2d.extent.width  = width;
          vk_rect2d.extent.height = height;
          gfx::rendering_attachment_t rendering_attachment{};
          rendering_attachment.handle_image_view =
              base->current_swapchain_image_view();
          rendering_attachment.image_layout =
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
          rendering_attachment.clear_value = {0, 0, 0, 0};
          rendering_attachment.load_op     = VK_ATTACHMENT_LOAD_OP_CLEAR;
          rendering_attachment.store_op    = VK_ATTACHMENT_STORE_OP_STORE;
          base->cmd_begin_rendering(cmd, {rendering_attachment}, std::nullopt,
                                    vk_rect2d);
          base->cmd_bind_graphics_pipeline(cmd, p, width, height);
          base->cmd_bind_descriptor_sets(cmd, p, 0, {ds});
          base->cmd_draw(cmd, 6, 1, 0, 0);
          base->cmd_end_rendering(cmd);
        })
        .add_write_image(base->current_swapchain_image(),
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rendergraph.add_pass([&](gfx::handle_commandbuffer_t cmd) {
      gfx::helper::imgui_newframe();
      ImGui::Begin("test");
      ImGui::End();
      gfx::helper::imgui_endframe(*context, cmd);
    });
    // empty pass to present swapchain
    rendergraph.add_pass([&](gfx::handle_commandbuffer_t cmd) {})
        .add_write_image(base->current_swapchain_image(), 0,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    base->render_rendergraph(rendergraph, cmd);

    base->end();
  }

  gfx::helper::imgui_shutdown();

  return 0;
}
