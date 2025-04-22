#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/math.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/types.hpp"
#include <vulkan/vulkan_core.h>

struct push_constant_t {
  VkDeviceAddress result;
};

int main(int argc, char **argv) {
  core::ref<core::window_t> window =
      core::make_ref<core::window_t>("test", 640, 420);
  core::ref<gfx::context_t> context = core::make_ref<gfx::context_t>(true);
  gfx::base_t base{window, context};

  gfx::handle_image_t image = gfx::helper::load_image_from_path_instant(
      *context, base._command_pool,
      "../../assets/textures/draw-3583548_1280.png", VK_FORMAT_R8G8B8A8_SRGB);
  gfx::handle_image_view_t image_view =
      context->create_image_view({.handle_image = image});

  gfx::handle_sampler_t sampler = context->create_sampler({});

  auto bimage = base.new_bindless_image();
  auto bsampler = base.new_bindless_sampler();
  horizon_info("bimage {}", bimage.val);
  horizon_info("bsampler {}", bsampler.val);

  base.set_bindless_image(bimage, image_view,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  base.set_bindless_sampler(bsampler, sampler);

  gfx::config_pipeline_layout_t cpl{};
  cpl.add_descriptor_set_layout(base._bindless_descriptor_set_layout);
  cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
  gfx::handle_pipeline_layout_t pl = context->create_pipeline_layout(cpl);

  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_shader(gfx::helper::create_slang_shader(
      *context, "../../assets/shaders/bindless/bindless.slang",
      gfx::shader_type_t::e_compute));
  gfx::handle_pipeline_t p = context->create_compute_pipeline(cp);

  gfx::config_buffer_t cb{};
  cb.vk_size = sizeof(core::vec4);
  cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
  gfx::handle_buffer_t b = context->create_buffer(cb);

  auto cbuf =
      gfx::helper::begin_single_use_commandbuffer(*context, base._command_pool);
  context->cmd_bind_pipeline(cbuf, p);
  context->cmd_bind_descriptor_sets(cbuf, p, 0,
                                    {base._bindless_descriptor_set});
  push_constant_t pc{};
  pc.result = context->get_buffer_device_address(b);
  context->cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                              sizeof(push_constant_t), &pc);
  context->cmd_dispatch(cbuf, 1, 1, 1);
  gfx::helper::end_single_use_command_buffer(*context, cbuf);
  core::vec4 *v = (core::vec4 *)context->map_buffer(b);
  horizon_info("{}", core::to_string(*v));
}
