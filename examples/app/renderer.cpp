#include "renderer.hpp"

#include "horizon/core/core.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/math.hpp"
#include "horizon/core/model.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/types.hpp"

#include "imgui.h"
#include <cstring>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

// Renderer Types
struct gpu_mesh_t {
  core::ref<gfx::context_t> ctx;
  gfx::handle_buffer_t vertex_buffer;
  gfx::handle_buffer_t index_buffer;
  uint32_t vertex_count;
  uint32_t index_count;
  // TODO: add material info

  gpu_mesh_t(core::ref<gfx::context_t> ctx) : ctx(ctx) {}

  gpu_mesh_t(const gpu_mesh_t &) = delete;
  gpu_mesh_t &operator=(const gpu_mesh_t &) = delete;

  gpu_mesh_t(gpu_mesh_t &&gpu_mesh) {
    ctx = gpu_mesh.ctx;
    vertex_buffer = gpu_mesh.vertex_buffer;
    index_buffer = gpu_mesh.index_buffer;
    vertex_count = gpu_mesh.vertex_count;
    index_count = gpu_mesh.index_count;
    gpu_mesh.ctx = nullptr;
  }
  gpu_mesh_t &operator=(gpu_mesh_t &&gpu_mesh) {
    ctx = gpu_mesh.ctx;
    vertex_buffer = gpu_mesh.vertex_buffer;
    index_buffer = gpu_mesh.index_buffer;
    vertex_count = gpu_mesh.vertex_count;
    index_count = gpu_mesh.index_count;
    gpu_mesh.ctx = nullptr;
    return *this;
  }

  ~gpu_mesh_t() {
    if (ctx) {
      ctx->wait_idle();
      ctx->destroy_buffer(vertex_buffer);
      ctx->destroy_buffer(index_buffer);
    }
  }
};

struct gpu_model_t {
  std::vector<gpu_mesh_t> meshes;
};

struct gpu_camera_t {
  core::mat4 view;
  core::mat4 inv_view;
  core::mat4 projection;
  core::mat4 inv_projection;
};

renderer_t::renderer_t(core::window_t &window, bool validation)
    : _window(window), _is_validating(validation) {

  _ctx = core::make_ref<gfx::context_t>(_is_validating);

  _swapchain_sampler = _ctx->create_sampler({});

  auto [width, height] = _window.dimensions();

  gfx::config_image_t config_final_image{};
  config_final_image.vk_width = width;
  config_final_image.vk_height = height;
  config_final_image.vk_depth = 1;
  config_final_image.vk_type = VK_IMAGE_TYPE_2D;
  config_final_image.vk_format = _final_image_format;
  config_final_image.vk_usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  config_final_image.vk_mips = 1;
  config_final_image.debug_name = "final image";
  _final_image = _ctx->create_image(config_final_image);
  _final_image_view = _ctx->create_image_view({.handle_image = _final_image});

  gfx::base_config_t base_config{.window = _window,
                                 .context = *_ctx,
                                 .sampler = _swapchain_sampler,
                                 .final_image_view = _final_image_view};
  _base = core::make_ref<gfx::base_t>(base_config);

  gfx::helper::imgui_init(_window, *_ctx, _base->_swapchain,
                          _final_image_format);

  gfx::config_buffer_t cb{};
  cb.vk_size = sizeof(gpu_camera_t);
  cb.debug_name = "camera buffer";
  cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  _camera_buffer =
      _base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);

  _diffuse = core::make_ref<diffuse_t>(*_base, _final_image_format);
}

renderer_t::~renderer_t() {
  _ctx->wait_idle();

  _ctx->destroy_image_view(_final_image_view);
  _ctx->destroy_image(_final_image);
  _ctx->destroy_sampler(_swapchain_sampler);

  gfx::helper::imgui_shutdown();
}

void renderer_t::begin(ecs::scene_t &scene, camera_t &camera) {
  // prepare
  scene.for_all<core::raw_model_t>([&](ecs::entity_id_t id,
                                       const core::raw_model_t &raw_model) {
    if (scene.has<gpu_model_t>(id))
      return;

    gpu_model_t &gpu_model = scene.construct<gpu_model_t>(id);

    for (auto &raw_mesh : raw_model.meshes) {
      gpu_mesh_t gpu_mesh{_ctx};
      gfx::config_buffer_t cb{};
      cb.vk_size = raw_mesh.vertices.size() * sizeof(raw_mesh.vertices[0]);
      cb.debug_name = "vertex buffer " + std::to_string(id);
      cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      cb.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
      gpu_mesh.vertex_buffer = gfx::helper::create_buffer_staged(
          *_ctx, _base->_command_pool, cb, raw_mesh.vertices.data(),
          cb.vk_size);
      gpu_mesh.vertex_count = raw_mesh.vertices.size();

      cb.vk_size = raw_mesh.indices.size() * sizeof(raw_mesh.indices[0]);
      cb.debug_name = "index buffer " + std::to_string(id);
      cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      cb.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
      gpu_mesh.index_buffer = gfx::helper::create_buffer_staged(
          *_ctx, _base->_command_pool, cb, raw_mesh.indices.data(), cb.vk_size);
      gpu_mesh.index_count = raw_mesh.indices.size();

      gpu_model.meshes.push_back(std::move(gpu_mesh));
    }
  });

  gpu_camera_t gpu_camera{};
  gpu_camera.view = camera.view();
  gpu_camera.inv_view = camera.inverse_view();
  gpu_camera.projection = camera.projection();
  gpu_camera.inv_projection = camera.inverse_projection();
  std::memcpy(_ctx->map_buffer(_base->buffer(_camera_buffer)), &gpu_camera,
              sizeof(gpu_camera));

  // render
  _base->begin();

  gfx::handle_commandbuffer_t cbuf = _base->current_commandbuffer();
  auto [width, height] = _window.dimensions();
  gfx::rendering_attachment_t color_rendering_attachment{};
  color_rendering_attachment.clear_value = {0, 0, 0, 0};
  color_rendering_attachment.image_layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
  color_rendering_attachment.handle_image_view = _final_image_view;

  // Transition image to color attachment optimal
  _ctx->cmd_image_memory_barrier(cbuf, _final_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

  // begin rendering
  _ctx->cmd_begin_rendering(
      cbuf, {color_rendering_attachment}, std::nullopt,
      VkRect2D{VkOffset2D{},
               {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}});

  // render objects
  auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);
  scene.for_all<gpu_model_t>([&](auto id, gpu_model_t& model) {
    push_constant_t pc;
    pc.camera = _ctx->get_buffer_device_address(_base->buffer(_camera_buffer));
    for (auto& mesh : model.meshes) {
      pc.vertices = _ctx->get_buffer_device_address(mesh.vertex_buffer);
      pc.indices = _ctx->get_buffer_device_address(mesh.index_buffer);
      _diffuse->render(cbuf, pc, mesh.index_count, viewport, scissor);
    }
  });

  // render gui
  gfx::helper::imgui_newframe();
  ImGui::Begin("Renderer Info");
  ImGui::Text("%f FPS", ImGui::GetIO().Framerate);
  ImGui::End();
}

void renderer_t::end() {
  gfx::handle_commandbuffer_t cbuf = _base->current_commandbuffer();
  gfx::helper::imgui_endframe(*_ctx, cbuf);

  // end rendering
  _ctx->cmd_end_rendering(cbuf);

  // transition image to shader read only optimal
  _ctx->cmd_image_memory_barrier(cbuf, _final_image,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  _base->end();
}
