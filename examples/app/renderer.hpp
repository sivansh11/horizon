#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "horizon/core/logger.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include <vulkan/vulkan_core.h>

namespace renderer {

struct push_constant_t {
  VkDeviceAddress num_rays;
  VkDeviceAddress trace_indirect_cmd;
  VkDeviceAddress ray_datas;
  VkDeviceAddress bvh;
  VkDeviceAddress triangles;
  VkDeviceAddress camera;
  VkDeviceAddress hits;
  uint32_t width;
  uint32_t height;
  uint32_t bounce_id;
};

struct raygen {
  raygen(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path = "../../assets/shaders/app/raygen.slang",
        .is_code = false,
        .name = "rayraygen compute",
        .type = gfx::shader_type_t::e_compute,
        .language = gfx::shader_language_t::e_slang,
        .debug_name = "rayraygen compute",
    };
    s = base._info.context.create_shader(cs);

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
    pl = base._info.context.create_pipeline_layout(cpl);

    gfx::config_pipeline_t cp{};
    cp.handle_pipeline_layout = pl;
    cp.add_shader(s);
    p = base._info.context.create_compute_pipeline(cp);

    t = base._info.context.create_timer({});
  }

  ~raygen() {
    base._info.context.destroy_pipeline(p);
    base._info.context.destroy_pipeline_layout(pl);
    base._info.context.destroy_shader(s);
  }

  void render(gfx::handle_commandbuffer_t cbuf, push_constant_t push_constant) {
    base._info.context.cmd_begin_timer(cbuf, t);
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch(cbuf, (push_constant.width + 8 - 1) / 8,
                                    (push_constant.height + 8 - 1) / 8, 1);
    base._info.context.cmd_end_timer(cbuf, t);
  }

  auto get_time() {
    if (auto time = base._info.context.timer_get_time(t)) {
      horizon_info("raygen took: {}", *time);
    }
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_timer_t t;
};

struct trace {
  trace(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path = "../../assets/shaders/app/trace.slang",
        .is_code = false,
        .name = "raytrace compute",
        .type = gfx::shader_type_t::e_compute,
        .language = gfx::shader_language_t::e_slang,
        .debug_name = "raytrace compute",
    };
    s = base._info.context.create_shader(cs);

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
    pl = base._info.context.create_pipeline_layout(cpl);

    gfx::config_pipeline_t cp{};
    cp.handle_pipeline_layout = pl;
    cp.add_shader(s);
    p = base._info.context.create_compute_pipeline(cp);

    t = base._info.context.create_timer({});
  }

  ~trace() {
    base._info.context.destroy_pipeline(p);
    base._info.context.destroy_pipeline_layout(pl);
    base._info.context.destroy_shader(s);
  }

  void render(gfx::handle_commandbuffer_t cbuf, gfx::handle_buffer_t buffer,
              push_constant_t push_constant) {
    base._info.context.cmd_begin_timer(cbuf, t);
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch_indirect(cbuf, buffer, 0);
    // base._info.context.cmd_dispatch(
    //     cbuf, ((push_constant.width * push_constant.height) + 32 - 1) / 32,
    //     1, 1);
    base._info.context.cmd_end_timer(cbuf, t);
  }

  auto get_time() {
    if (auto time = base._info.context.timer_get_time(t)) {
      horizon_info("trace took: {}", *time);
    }
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_timer_t t;
};

// TODO: maybe make this a full screen pass ?
struct shade {
  shade(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path = "../../assets/shaders/app/shade.slang",
        .is_code = false,
        .name = "raytrace compute",
        .type = gfx::shader_type_t::e_compute,
        .language = gfx::shader_language_t::e_slang,
        .debug_name = "raytrace compute",
    };
    s = base._info.context.create_shader(cs);

    gfx::config_descriptor_set_layout_t cdsl{};
    cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            VK_SHADER_STAGE_ALL);
    dsl = base._info.context.create_descriptor_set_layout(cdsl);

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_descriptor_set_layout(dsl);
    cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
    pl = base._info.context.create_pipeline_layout(cpl);

    gfx::config_pipeline_t cp{};
    cp.handle_pipeline_layout = pl;
    cp.add_shader(s);
    p = base._info.context.create_compute_pipeline(cp);

    ds = base._info.context.allocate_descriptor_set(
        {.handle_descriptor_set_layout = dsl});

    t = base._info.context.create_timer({});
  }

  ~shade() {
    base._info.context.free_descriptor_set(ds);
    base._info.context.destroy_pipeline(p);
    base._info.context.destroy_pipeline_layout(pl);
    base._info.context.destroy_descriptor_set_layout(dsl);
    base._info.context.destroy_shader(s);
  }

  void update_descriptor_set(gfx::handle_image_view_t image_view) {
    base._info.context.update_descriptor_set(ds)
        .push_image_write(0, {.handle_sampler = gfx::null_handle,
                              .handle_image_view = image_view,
                              .vk_image_layout = VK_IMAGE_LAYOUT_GENERAL})
        .commit();
  }

  void render(gfx::handle_commandbuffer_t cbuf, push_constant_t push_constant) {
    base._info.context.cmd_begin_timer(cbuf, t);
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_bind_descriptor_sets(cbuf, p, 0, {ds});
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch(cbuf, (push_constant.width + 8 - 1) / 8,
                                    (push_constant.height + 8 - 1) / 8, 1);
    base._info.context.cmd_end_timer(cbuf, t);
  }

  auto get_time() {
    if (auto time = base._info.context.timer_get_time(t)) {
      horizon_info("shade took: {}", *time);
    }
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_descriptor_set_layout_t dsl;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_descriptor_set_t ds;
  gfx::handle_timer_t t;
};

} // namespace renderer

#endif
