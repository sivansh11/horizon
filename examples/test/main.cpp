#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"

int main(int argc, char **argv) {
  gfx::context_t ctx{true};

  gfx::config_pipeline_layout_t cpl{};
  cpl.add_push_constant(2 * sizeof(VkDeviceAddress),
                        VK_SHADER_STAGE_COMPUTE_BIT);
  gfx::handle_pipeline_layout_t pl = ctx.create_pipeline_layout(cpl);

  gfx::config_shader_t cs{};
  cs.name = "test.slang";
  cs.type = gfx::shader_type_t::e_compute;
  cs.is_code = false;
  cs.language = gfx::shader_language_t::e_slang;
  cs.code_or_path = "../../assets/shaders/test/test.slang";
  gfx::handle_shader_t s = ctx.create_shader(cs);

  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_shader(s);
  gfx::handle_pipeline_t p = ctx.create_compute_pipeline(cp);

  gfx::config_buffer_t cb{};
  cb.vk_size = 2 * sizeof(uint);
  cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
  gfx::handle_buffer_t b = ctx.create_buffer(cb);

  uint *data = reinterpret_cast<uint *>(ctx.map_buffer(b));
  data[0] = 5;
  data[1] = 0;

  std::cout << data[0] << ' ' << data[1] << '\n';

  VkDeviceAddress pc[2] = {
      ctx.get_buffer_device_address(b),
      ctx.get_buffer_device_address(b) + 4 // sizeof uint == 4
  };

  gfx::handle_command_pool_t command_pool = ctx.create_command_pool({});
  gfx::handle_commandbuffer_t cbuf =
      gfx::helper::begin_single_use_commandbuffer(ctx, command_pool);
  ctx.cmd_bind_pipeline(cbuf, p);
  ctx.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         2 * sizeof(VkDeviceAddress), pc);
  ctx.cmd_dispatch(cbuf, 1, 1, 1);
  gfx::helper::end_single_use_command_buffer(ctx, cbuf);

  std::cout << data[0] << ' ' << data[1] << '\n';

  return 0;
}
