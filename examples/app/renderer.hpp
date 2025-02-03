#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "camera.hpp"

#include "horizon/core/ecs.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/types.hpp"

struct push_constant_t {
  VkDeviceAddress vertices;
  VkDeviceAddress indices;
  VkDeviceAddress camera;
};

struct diffuse_t {
  diffuse_t(gfx::base_t &base, VkFormat format) : base(base) {
    gfx::config_shader_t cvs{
        .code_or_path = "../../assets/shaders/app/vert.slang",
        .is_code = false,
        .name = "diffuse",
        .type = gfx::shader_type_t::e_vertex,
        .language = gfx::shader_language_t::e_slang,
        .debug_name = "vert",
    };
    vs = base._info.context.create_shader(cvs);

    gfx::config_shader_t cfs{
        .code_or_path = "../../assets/shaders/app/frag.slang",
        .is_code = false,
        .name = "diffuse",
        .type = gfx::shader_type_t::e_fragment,
        .language = gfx::shader_language_t::e_slang,
        .debug_name = "frag",
    };
    fs = base._info.context.create_shader(cfs);

    gfx::config_descriptor_set_layout_t cdsl{};
    dsl = base._info.context.create_descriptor_set_layout(cdsl);

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_descriptor_set_layout(dsl);
    cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
    pl = base._info.context.create_pipeline_layout(cpl);

    gfx::config_pipeline_t cp{};
    cp.handle_pipeline_layout = pl;
    cp.add_color_attachment(format, gfx::default_color_blend_attachment());
    cp.add_shader(vs);
    cp.add_shader(fs);
    p = base._info.context.create_graphics_pipeline(cp);

    ds = base._info.context.allocate_descriptor_set(
        {.handle_descriptor_set_layout = dsl});
  }

  ~diffuse_t() {
    base._info.context.destroy_pipeline(p);
    base._info.context.destroy_pipeline_layout(pl);
    base._info.context.destroy_shader(fs);
    base._info.context.destroy_shader(vs);
  }

  void update_descriptor_set(gfx::handle_image_view_t image_view) {
    base._info.context.update_descriptor_set(ds)
        .push_image_write(0, {.handle_sampler = core::null_handle,
                              .handle_image_view = image_view,
                              .vk_image_layout = VK_IMAGE_LAYOUT_GENERAL})
        .commit();
  }

  void render(gfx::handle_commandbuffer_t cbuf, push_constant_t push_constant,
              uint32_t index_count, auto viewport, auto scissor) {
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_set_viewport_and_scissor(cbuf, viewport, scissor);
    // base._info.context.cmd_bind_descriptor_sets(cbuf, p, 0, {ds});
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    // base._info.context.cmd_draw_indexed(cbuf, index_count, 1, 0, 0, 0);
    base._info.context.cmd_draw(cbuf, index_count, 1, 0, 0);
  }

  gfx::base_t &base;

  gfx::handle_shader_t vs;
  gfx::handle_shader_t fs;
  gfx::handle_descriptor_set_layout_t dsl;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_descriptor_set_t ds;
};

class renderer_t {
public:
  renderer_t(core::window_t &window, bool validations);
  ~renderer_t();

  void begin(ecs::scene_t &scene, camera_t &camera);
  void end();

private:
  core::window_t &_window;
  const bool _is_validating;

  core::ref<gfx::context_t> _ctx;

  gfx::handle_sampler_t _swapchain_sampler;
  VkFormat _final_image_format = VK_FORMAT_R8G8B8A8_SRGB;
  gfx::handle_image_t _final_image;
  gfx::handle_image_view_t _final_image_view;

  core::ref<gfx::base_t> _base;

  gfx::handle_managed_buffer_t _camera_buffer;

  core::ref<diffuse_t> _diffuse;
};

#endif
