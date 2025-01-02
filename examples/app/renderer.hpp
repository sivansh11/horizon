#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "horizon/core/bvh.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/model.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "imgui.h"

#include <vulkan/vulkan_core.h>

#include <filesystem>
#include <vector>

namespace renderer {

struct ray_data_t {
  core::vec3 origin, direction;
  core::vec3 inverse_direction;
  float tmin, tmax;
  uint32_t px_id;
};
static_assert(sizeof(ray_data_t) == 48, "sizeof(ray_data_t) != 48");

struct triangle_t {
  // core::vec3 v0, v1, v2;
  core::vertex_t v0, v1, v2;
};

struct bvh_t {
  VkDeviceAddress nodes;
  VkDeviceAddress primitive_indices;
};

static const float infinity = 100000000000000.f;
static const uint32_t invalid_index = 4294967295u;

struct hit_t {
  uint32_t primitive_id = invalid_index;
  float t = infinity;
  float u = 0, v = 0, w = 0;
};

struct camera_t {
  core::mat4 inverse_projection;
  core::mat4 inverse_view;
};

struct push_constant_t {
  VkDeviceAddress throughput;
  VkDeviceAddress num_rays;
  VkDeviceAddress new_num_rays;
  VkDeviceAddress trace_indirect_cmd;
  VkDeviceAddress ray_datas;
  VkDeviceAddress new_ray_datas;
  VkDeviceAddress bvh;
  VkDeviceAddress triangles;
  VkDeviceAddress camera;
  VkDeviceAddress hits;
  uint32_t width;
  uint32_t height;
  uint32_t bounce_id;
};

struct raygen_t {
  raygen_t(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path = "../../assets/shaders/app/raygen.slang",
        .is_code = false,
        .name = "rayraygen compute",
        .type = gfx::shader_type_t::e_compute,
        .language = gfx::shader_language_t::e_slang,
        .debug_name = "rayraygen compute",
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
  }

  ~raygen_t() {
    base._info.context.destroy_pipeline(p);
    base._info.context.destroy_pipeline_layout(pl);
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
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_bind_descriptor_sets(cbuf, p, 0, {ds});
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch(cbuf, (push_constant.width + 8 - 1) / 8,
                                    (push_constant.height + 8 - 1) / 8, 1);
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_descriptor_set_layout_t dsl;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_descriptor_set_t ds;
};

struct trace_t {
  trace_t(gfx::base_t &base) : base(base) {
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
  }

  ~trace_t() {
    base._info.context.destroy_pipeline(p);
    base._info.context.destroy_pipeline_layout(pl);
    base._info.context.destroy_shader(s);
  }

  void render(gfx::handle_commandbuffer_t cbuf, gfx::handle_buffer_t buffer,
              push_constant_t push_constant) {
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch_indirect(cbuf, buffer, 0);
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
};

struct shade_t {
  shade_t(gfx::base_t &base) : base(base) {
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
  }

  ~shade_t() {
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

  void render(gfx::handle_commandbuffer_t cbuf, gfx::handle_buffer_t buffer,
              push_constant_t push_constant) {
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_bind_descriptor_sets(cbuf, p, 0, {ds});
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch_indirect(cbuf, buffer, 0);
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_descriptor_set_layout_t dsl;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_descriptor_set_t ds;
};

struct debug_color_t {
  debug_color_t(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path = "../../assets/shaders/app/debug_shade_color.slang",
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
  }

  ~debug_color_t() {
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

  void render(gfx::handle_commandbuffer_t cbuf, gfx::handle_buffer_t buffer,
              push_constant_t push_constant) {
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_bind_descriptor_sets(cbuf, p, 0, {ds});
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch_indirect(cbuf, buffer, 0);
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_descriptor_set_layout_t dsl;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_descriptor_set_t ds;
};

struct debug_heatmap_node_intersections_t {
  debug_heatmap_node_intersections_t(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path = "../../assets/shaders/app/"
                        "debug_shade_heatmap_node_intersections.slang",
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
  }

  ~debug_heatmap_node_intersections_t() {
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

  void render(gfx::handle_commandbuffer_t cbuf, gfx::handle_buffer_t buffer,
              push_constant_t push_constant) {
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_bind_descriptor_sets(cbuf, p, 0, {ds});
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch_indirect(cbuf, buffer, 0);
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_descriptor_set_layout_t dsl;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_descriptor_set_t ds;
};

struct debug_heatmap_primitive_intersections_t {
  debug_heatmap_primitive_intersections_t(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path = "../../assets/shaders/app/"
                        "debug_shade_heatmap_primitive_intersections.slang",
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
  }

  ~debug_heatmap_primitive_intersections_t() {
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

  void render(gfx::handle_commandbuffer_t cbuf, gfx::handle_buffer_t buffer,
              push_constant_t push_constant) {
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_bind_descriptor_sets(cbuf, p, 0, {ds});
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch_indirect(cbuf, buffer, 0);
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_descriptor_set_layout_t dsl;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
  gfx::handle_descriptor_set_t ds;
};

struct write_indirect_dispatch_t {
  write_indirect_dispatch_t(gfx::base_t &base) : base(base) {
    gfx::config_shader_t cs{
        .code_or_path =
            "../../assets/shaders/app/write_indirect_dispatch.slang",
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
  }

  ~write_indirect_dispatch_t() {
    base._info.context.destroy_pipeline(p);
    base._info.context.destroy_pipeline_layout(pl);
    base._info.context.destroy_shader(s);
  }

  void render(gfx::handle_commandbuffer_t cbuf, push_constant_t push_constant) {
    base._info.context.cmd_bind_pipeline(cbuf, p);
    base._info.context.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                          sizeof(push_constant_t),
                                          &push_constant);
    base._info.context.cmd_dispatch(cbuf, 1, 1, 1);
  }

  gfx::base_t &base;

  gfx::handle_shader_t s;
  gfx::handle_pipeline_layout_t pl;
  gfx::handle_pipeline_t p;
};

struct gpu_timer_t {
  gpu_timer_t(gfx::base_t &base, bool enable) : _base(base), enable(enable) {}

  void clear() {
    _base._info.context.wait_idle();
    for (auto [name, handle] : timers) {
      _base._info.context.destroy_timer(handle);
    }
    timers.clear();
  }

  void start(gfx::handle_commandbuffer_t cbuf, std::string name) {
    if (!enable)
      return;
    if (!timers.contains(name)) {
      timers[name] = _base._info.context.create_timer({});
    }
    _base._info.context.cmd_begin_timer(cbuf, timers[name]);
  }

  void end(gfx::handle_commandbuffer_t cbuf, std::string name) {
    if (!enable)
      return;
    _base._info.context.cmd_end_timer(cbuf, timers[name]);
  }

  std::map<std::string, float> get_times() {
    if (!enable)
      return {};
    std::map<std::string, float> res{};
    for (auto [name, handle] : timers) {
      auto time = _base._info.context.timer_get_time(handle);
      if (time)
        res[name] = *time;
    }
    return res;
  }

  gfx::base_t &_base;
  bool enable;
  std::map<std::string, gfx::handle_timer_t> timers{};
};

enum class renderer_type_t : int32_t {
  e_normal = 0,
  e_debug_color,
  e_debug_heatmap_node_intersections,
  e_debug_heatmap_primitive_intersections,
};

struct renderer_t {
  // TODO: have a proper renderer scene thing and remove this mesh shit
  renderer_t(core::window_t &window, bool validations,
             const std::filesystem::path &model_path)
      : _window(window), _ctx(core::make_ref<gfx::context_t>(validations)) {

    _global_sampler = _ctx->create_sampler({});

    auto [width, height] = _window.dimensions();

    gfx::config_image_t config_final_image{};
    config_final_image.vk_width = width;
    config_final_image.vk_height = height;
    config_final_image.vk_depth = 1;
    config_final_image.vk_type = VK_IMAGE_TYPE_2D;
    config_final_image.vk_format = _final_image_format;
    config_final_image.vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                  VK_IMAGE_USAGE_STORAGE_BIT;
    config_final_image.vk_mips = 1;
    config_final_image.debug_name = "final image";
    _final_image = _ctx->create_image(config_final_image);
    _final_image_view = _ctx->create_image_view({.handle_image = _final_image});

    gfx::base_config_t base_config{.window = _window,
                                   .context = *_ctx,
                                   .sampler = _global_sampler,
                                   .final_image_view = _final_image_view};
    _base = core::make_ref<gfx::base_t>(base_config);

    gfx::helper::imgui_init(_window, *_ctx, _base->_swapchain,
                            _final_image_format);

    std::vector<core::aabb_t> aabbs{};
    std::vector<core::vec3> centers{};
    std::vector<triangle_t> triangles{};

    core::model_t model = core::load_model_from_path(model_path);

    // TODO: cache the bvh creation, create an asset loader with a common cached
    // asset folder
    for (auto mesh : model.meshes) {
      for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
        triangle_t triangle{
            .v0 = mesh.vertices[mesh.indices[i + 0]],
            .v1 = mesh.vertices[mesh.indices[i + 1]],
            .v2 = mesh.vertices[mesh.indices[i + 2]],
        };
        core::aabb_t aabb{};
        aabb.grow(triangle.v0.position)
            .grow(triangle.v1.position)
            .grow(triangle.v2.position);
        core::vec3 center{};
        center = (triangle.v0.position + triangle.v1.position +
                  triangle.v2.position) /
                 3.f;

        triangles.push_back(triangle);
        aabbs.push_back(aabb);
        centers.push_back(center);
      }
    }

    core::bvh::options_t options{
        .o_min_primitive_count = 1, // TODO: try 0
        .o_max_primitive_count = 1,
        .o_object_split_search_type =
            core::bvh::object_split_search_type_t::e_binned_sah,
        .o_primitive_intersection_cost = 1.1f,
        .o_node_intersection_cost = 1.f,
        .o_samples = 100,
    };

    core::bvh::bvh_t bvh = core::bvh::build_bvh2(aabbs.data(), centers.data(),
                                                 triangles.size(), options);

    horizon_info("cost of bvh {}", core::bvh::cost_of_bvh(bvh, options));
    horizon_info("depth of bvh {}", core::bvh::depth_of_bvh(bvh));
    horizon_info("nodes {}", bvh.nodes.size());
    horizon_info("{}", uint32_t(bvh.nodes[0].primitive_count));

    core::bvh::collapse_nodes(bvh, options);

    horizon_info("cost of bvh {}", core::bvh::cost_of_bvh(bvh, options));
    horizon_info("depth of bvh {}", core::bvh::depth_of_bvh(bvh));
    horizon_info("nodes {}", bvh.nodes.size());
    horizon_info("{}", uint32_t(bvh.nodes[0].primitive_count));

    _raygen = core::make_ref<raygen_t>(*_base);
    _trace = core::make_ref<trace_t>(*_base);
    _shade = core::make_ref<shade_t>(*_base);
    _debug_color = core::make_ref<debug_color_t>(*_base);
    _debug_heatmap_node_intersections =
        core::make_ref<debug_heatmap_node_intersections_t>(*_base);
    _debug_heatmap_primitive_intersections =
        core::make_ref<debug_heatmap_primitive_intersections_t>(*_base);
    _write_indirect_dispatch =
        core::make_ref<write_indirect_dispatch_t>(*_base);
    _raygen->update_descriptor_set(_final_image_view);
    _shade->update_descriptor_set(_final_image_view);
    _debug_color->update_descriptor_set(_final_image_view);
    _debug_heatmap_node_intersections->update_descriptor_set(_final_image_view);
    _debug_heatmap_primitive_intersections->update_descriptor_set(
        _final_image_view);

    gfx::config_buffer_t config_throughput_buffer{};
    config_throughput_buffer.vk_size = sizeof(glm::vec3) * width * height;
    config_throughput_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_throughput_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _throughput_buffer = _ctx->create_buffer(config_throughput_buffer);

    gfx::config_buffer_t config_num_rays_buffer{};
    config_num_rays_buffer.vk_size = sizeof(uint32_t);
    config_num_rays_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_num_rays_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _num_rays_buffer = _ctx->create_buffer(config_num_rays_buffer);

    gfx::config_buffer_t config_new_num_rays_buffer{};
    config_new_num_rays_buffer.vk_size = sizeof(uint32_t);
    config_new_num_rays_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_new_num_rays_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _new_num_rays_buffer = _ctx->create_buffer(config_new_num_rays_buffer);

    gfx::config_buffer_t config_dispatch_indirect_buffer{};
    config_dispatch_indirect_buffer.vk_size = sizeof(VkDispatchIndirectCommand);
    config_dispatch_indirect_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_dispatch_indirect_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    _dispatch_indirect_buffer =
        _ctx->create_buffer(config_dispatch_indirect_buffer);

    gfx::config_buffer_t config_ray_datas_buffer{};
    config_ray_datas_buffer.vk_size = sizeof(ray_data_t) * width * height;
    config_ray_datas_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_ray_datas_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _ray_datas_buffer = _ctx->create_buffer(config_ray_datas_buffer);

    gfx::config_buffer_t config_new_ray_datas_buffer{};
    config_new_ray_datas_buffer.vk_size = sizeof(ray_data_t) * width * height;
    config_new_ray_datas_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_new_ray_datas_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _new_ray_datas_buffer = _ctx->create_buffer(config_new_ray_datas_buffer);

    gfx::config_buffer_t config_nodes_buffer{};
    config_nodes_buffer.vk_size = sizeof(core::bvh::node_t) * bvh.nodes.size();
    config_nodes_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_nodes_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _nodes_buffer = gfx::helper::create_buffer_staged(
        *_ctx, _base->_command_pool, config_nodes_buffer, bvh.nodes.data(),
        sizeof(core::bvh::node_t) * bvh.nodes.size());

    gfx::config_buffer_t config_primitive_indices_buffer{};
    config_primitive_indices_buffer.vk_size =
        sizeof(uint32_t) * bvh.primitive_indices.size();
    config_primitive_indices_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_primitive_indices_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _primitive_indices_buffer = gfx::helper::create_buffer_staged(
        *_ctx, _base->_command_pool, config_primitive_indices_buffer,
        bvh.primitive_indices.data(),
        sizeof(uint32_t) * bvh.primitive_indices.size());

    bvh_t _bvh{};
    _bvh.nodes = _ctx->get_buffer_device_address(_nodes_buffer);
    _bvh.primitive_indices =
        _ctx->get_buffer_device_address(_primitive_indices_buffer);
    gfx::config_buffer_t config_bvh_buffer{};
    config_bvh_buffer.vk_size = sizeof(bvh_t);
    config_bvh_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    config_bvh_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _bvh_buffer = gfx::helper::create_buffer_staged(
        *_ctx, _base->_command_pool, config_bvh_buffer, &_bvh, sizeof(bvh_t));

    gfx::config_buffer_t config_triangles_buffer{};
    config_triangles_buffer.vk_size = sizeof(triangle_t) * triangles.size();
    config_triangles_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    config_triangles_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _triangles_buffer = gfx::helper::create_buffer_staged(
        *_ctx, _base->_command_pool, config_triangles_buffer, triangles.data(),
        sizeof(triangle_t) * triangles.size());

    gfx::config_buffer_t config_camera_buffer{};
    config_camera_buffer.vk_size = sizeof(camera_t);
    config_camera_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    config_camera_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _camera_buffer = _base->create_buffer(
        gfx::resource_update_policy_t::e_every_frame, config_camera_buffer);

    gfx::config_buffer_t config_hits_buffer{};
    config_hits_buffer.vk_size =
        sizeof(hit_t) * width * height * 1.5f; // 1.5f for extra debug info
    config_hits_buffer.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    config_hits_buffer.vk_buffer_usage_flags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    _hits_buffer = _ctx->create_buffer(config_hits_buffer);

    _gpu_timer = core::make_ref<gpu_timer_t>(*_base, false);
  }

  ~renderer_t() {
    _ctx->wait_idle();

    _ctx->destroy_image_view(_final_image_view);
    _ctx->destroy_image(_final_image);
    _ctx->destroy_sampler(_global_sampler);

    gfx::helper::imgui_shutdown();
  }

  void render_frame(const camera_t &camera) {
    auto [width, height] = _window.dimensions();
    _base->begin();

    auto cbuf = _base->current_commandbuffer();

    *reinterpret_cast<camera_t *>(
        _ctx->map_buffer(_base->buffer(_camera_buffer))) = camera;

    _ctx->cmd_image_memory_barrier(
        cbuf, _final_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // dispatch work

    // TODO: clear image
    renderer::push_constant_t push_constant{};
    push_constant.throughput =
        _ctx->get_buffer_device_address(_throughput_buffer);
    push_constant.num_rays = _ctx->get_buffer_device_address(_num_rays_buffer);
    push_constant.new_num_rays =
        _ctx->get_buffer_device_address(_new_num_rays_buffer);
    push_constant.trace_indirect_cmd =
        _ctx->get_buffer_device_address(_dispatch_indirect_buffer);
    push_constant.ray_datas =
        _ctx->get_buffer_device_address(_ray_datas_buffer);
    push_constant.new_ray_datas =
        _ctx->get_buffer_device_address(_new_ray_datas_buffer);
    push_constant.bvh = _ctx->get_buffer_device_address(_bvh_buffer);
    push_constant.triangles =
        _ctx->get_buffer_device_address(_triangles_buffer);
    push_constant.camera =
        _ctx->get_buffer_device_address(_base->buffer(_camera_buffer));
    push_constant.hits = _ctx->get_buffer_device_address(_hits_buffer);
    push_constant.width = width;
    push_constant.height = height;
    push_constant.bounce_id = 0;

    _gpu_timer->start(cbuf, "raygen");
    _raygen->render(cbuf, push_constant);
    _gpu_timer->end(cbuf, "raygen");
    _ctx->cmd_buffer_memory_barrier(
        cbuf, _ray_datas_buffer,
        _ctx->get_buffer(_ray_datas_buffer).config.vk_size, 0,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    _ctx->cmd_buffer_memory_barrier(
        cbuf, _new_num_rays_buffer,
        _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    _ctx->cmd_buffer_memory_barrier(
        cbuf, _num_rays_buffer,
        _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    _ctx->cmd_buffer_memory_barrier(
        cbuf, _dispatch_indirect_buffer,
        _ctx->get_buffer(_dispatch_indirect_buffer).config.vk_size, 0,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
    _ctx->cmd_image_memory_barrier(
        cbuf, _final_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    switch (type) {
    case renderer_type_t::e_normal: {
      for (uint32_t bounce_id = 0; bounce_id <= _max_bounces; bounce_id++) {
        push_constant.bounce_id = bounce_id;
        _gpu_timer->start(cbuf, "trace" + std::to_string(bounce_id));
        _trace->render(cbuf, _dispatch_indirect_buffer, push_constant);
        _gpu_timer->end(cbuf, "trace" + std::to_string(bounce_id));
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _new_num_rays_buffer,
            _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _num_rays_buffer,
            _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _hits_buffer, _ctx->get_buffer(_hits_buffer).config.vk_size,
            0, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _gpu_timer->start(cbuf, "shade" + std::to_string(bounce_id));
        _shade->render(cbuf, _dispatch_indirect_buffer, push_constant);
        _gpu_timer->end(cbuf, "shade" + std::to_string(bounce_id));
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _ray_datas_buffer,
            _ctx->get_buffer(_ray_datas_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _new_ray_datas_buffer,
            _ctx->get_buffer(_new_ray_datas_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _throughput_buffer,
            _ctx->get_buffer(_throughput_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _new_num_rays_buffer,
            _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _num_rays_buffer,
            _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _ctx->cmd_image_memory_barrier(
            cbuf, _final_image, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        _gpu_timer->start(cbuf, "write_indirect_dispatch" +
                                    std::to_string(bounce_id));
        _write_indirect_dispatch->render(cbuf, push_constant);
        _gpu_timer->end(cbuf,
                        "write_indirect_dispatch" + std::to_string(bounce_id));
        _ctx->cmd_buffer_memory_barrier(
            cbuf, _dispatch_indirect_buffer,
            _ctx->get_buffer(_dispatch_indirect_buffer).config.vk_size, 0,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
        std::swap(push_constant.ray_datas, push_constant.new_ray_datas);
        std::swap(push_constant.num_rays, push_constant.new_num_rays);
      }
    } break;

    case renderer_type_t::e_debug_color: {
      _gpu_timer->start(cbuf, "trace");
      _trace->render(cbuf, _dispatch_indirect_buffer, push_constant);
      _gpu_timer->end(cbuf, "trace");
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_num_rays_buffer,
          _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _num_rays_buffer,
          _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _hits_buffer, _ctx->get_buffer(_hits_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _gpu_timer->start(cbuf, "shade");
      _debug_color->render(cbuf, _dispatch_indirect_buffer, push_constant);
      _gpu_timer->end(cbuf, "shade");
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _ray_datas_buffer,
          _ctx->get_buffer(_ray_datas_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_ray_datas_buffer,
          _ctx->get_buffer(_new_ray_datas_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _throughput_buffer,
          _ctx->get_buffer(_throughput_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_num_rays_buffer,
          _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _num_rays_buffer,
          _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_image_memory_barrier(
          cbuf, _final_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    } break;

    case renderer_type_t::e_debug_heatmap_node_intersections: {
      _gpu_timer->start(cbuf, "trace");
      _trace->render(cbuf, _dispatch_indirect_buffer, push_constant);
      _gpu_timer->end(cbuf, "trace");
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_num_rays_buffer,
          _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _num_rays_buffer,
          _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _hits_buffer, _ctx->get_buffer(_hits_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _gpu_timer->start(cbuf, "shade");
      _debug_heatmap_node_intersections->render(cbuf, _dispatch_indirect_buffer,
                                                push_constant);
      _gpu_timer->end(cbuf, "shade");
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _ray_datas_buffer,
          _ctx->get_buffer(_ray_datas_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_ray_datas_buffer,
          _ctx->get_buffer(_new_ray_datas_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _throughput_buffer,
          _ctx->get_buffer(_throughput_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_num_rays_buffer,
          _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _num_rays_buffer,
          _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_image_memory_barrier(
          cbuf, _final_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    } break;

    case renderer_type_t::e_debug_heatmap_primitive_intersections: {
      _gpu_timer->start(cbuf, "trace");
      _trace->render(cbuf, _dispatch_indirect_buffer, push_constant);
      _gpu_timer->end(cbuf, "trace");
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_num_rays_buffer,
          _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _num_rays_buffer,
          _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _hits_buffer, _ctx->get_buffer(_hits_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _gpu_timer->start(cbuf, "shade");
      _debug_heatmap_primitive_intersections->render(
          cbuf, _dispatch_indirect_buffer, push_constant);
      _gpu_timer->end(cbuf, "shade");
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _ray_datas_buffer,
          _ctx->get_buffer(_ray_datas_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_ray_datas_buffer,
          _ctx->get_buffer(_new_ray_datas_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _throughput_buffer,
          _ctx->get_buffer(_throughput_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _new_num_rays_buffer,
          _ctx->get_buffer(_new_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_buffer_memory_barrier(
          cbuf, _num_rays_buffer,
          _ctx->get_buffer(_num_rays_buffer).config.vk_size, 0,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      _ctx->cmd_image_memory_barrier(
          cbuf, _final_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    } break;
    }

    // Transition image to color attachment optimal
    _ctx->cmd_image_memory_barrier(
        cbuf, _final_image, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    gfx::rendering_attachment_t color_rendering_attachment{};
    color_rendering_attachment.clear_value = {0, 0, 0, 0};
    color_rendering_attachment.image_layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
    color_rendering_attachment.handle_image_view = _final_image_view;

    // begin rendering
    _ctx->cmd_begin_rendering(cbuf, {color_rendering_attachment}, std::nullopt,
                              VkRect2D{VkOffset2D{},
                                       {static_cast<uint32_t>(width),
                                        static_cast<uint32_t>(height)}});
    bool clear_gpu_timer = false;
    gfx::helper::imgui_newframe();
    ImGui::Begin("debug");
    ImGui::Text("%f", ImGui::GetIO().Framerate);
    if (ImGui::Button("-")) {
      clear_gpu_timer = true;
      _max_bounces -= 1;
      if (_max_bounces <= 1)
        _max_bounces = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("+")) {
      clear_gpu_timer = true;
      _max_bounces += 1;
      if (_max_bounces >= 100)
        _max_bounces = 100;
    }
    ImGui::SameLine();
    if (ImGui::SliderInt("bounces", &_max_bounces, 1, 100)) {
      clear_gpu_timer = true;
    }
    const char *items[] = {"normal", "debug_color",
                           "debug_heatmap_node_intersections",
                           "debug_heatmap_primitive_intersections"};
    if (ImGui::Combo("select renderer", reinterpret_cast<int *>(&type), items,
                     4)) {
      clear_gpu_timer = true;
    }
    if (ImGui::Checkbox("enable timer", &_gpu_timer->enable)) {
      clear_gpu_timer = true;
    }
    for (auto [name, time] : _gpu_times) {
      ImGui::Text("%s took %fms", name.c_str(), time);
    }
    ImGui::End();
    gfx::helper::imgui_endframe(*_ctx, cbuf);

    // end rendering
    _ctx->cmd_end_rendering(cbuf);

    // transition image to shader read only optimal
    _ctx->cmd_image_memory_barrier(
        cbuf, _final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    _base->end();

    if (clear_gpu_timer)
      _gpu_timer->clear();

    _gpu_times = _gpu_timer->get_times();
  }

  // TODO: create a proper scene from a renderer pov

private:
  core::window_t &_window;
  core::ref<gfx::context_t> _ctx;

  VkFormat _final_image_format = VK_FORMAT_R32G32B32A32_SFLOAT;

  gfx::handle_sampler_t _global_sampler;
  gfx::handle_image_t _final_image;
  gfx::handle_image_view_t _final_image_view;

  core::ref<gfx::base_t> _base;

  core::ref<raygen_t> _raygen;
  core::ref<trace_t> _trace;
  core::ref<shade_t> _shade;
  core::ref<debug_color_t> _debug_color;
  core::ref<debug_heatmap_node_intersections_t>
      _debug_heatmap_node_intersections;
  core::ref<debug_heatmap_primitive_intersections_t>
      _debug_heatmap_primitive_intersections;
  core::ref<write_indirect_dispatch_t> _write_indirect_dispatch;

  gfx::handle_buffer_t _throughput_buffer;
  gfx::handle_buffer_t _num_rays_buffer;
  gfx::handle_buffer_t _new_num_rays_buffer;
  gfx::handle_buffer_t _dispatch_indirect_buffer;
  gfx::handle_buffer_t _ray_datas_buffer;
  gfx::handle_buffer_t _new_ray_datas_buffer;
  gfx::handle_buffer_t _bvh_buffer;
  gfx::handle_buffer_t _nodes_buffer;
  gfx::handle_buffer_t _primitive_indices_buffer;
  gfx::handle_buffer_t _triangles_buffer;
  gfx::handle_managed_buffer_t _camera_buffer;
  gfx::handle_buffer_t _hits_buffer;

  core::ref<gpu_timer_t> _gpu_timer;
  std::map<std::string, float> _gpu_times{};

  // options
  int32_t _max_bounces = 3;
  renderer_type_t type = renderer_type_t::e_normal;
};

} // namespace renderer

#endif
