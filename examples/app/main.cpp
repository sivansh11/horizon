#include "horizon/core/bvh.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/model.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"

#include "renderer.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <vector>

struct ray_t {
  core::vec3 origin;
  core::vec3 direction;
};
static_assert(sizeof(ray_t) == 24, "sizeof(ray_t) != 24");

struct ray_data_t {
  core::vec3 origin, direction;
  core::vec3 inverse_direction;
  float tmin, tmax;
};
static_assert(sizeof(ray_data_t) == 44, "sizeof(ray_data_t) != 44");

struct triangle_t {
  core::vec3 v0, v1, v2;
};
static_assert(sizeof(triangle_t) == 36, "sizeof(triangle_t) != 36");

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

class editor_camera_t {
public:
  editor_camera_t(core::window_t &window) : _window(window) {}
  void update_projection(float aspect_ratio);
  void update(float ts);

  const core::mat4 &projection() const { return _projection; }
  const core::mat4 &view() const { return _view; }
  // creates a new mat4
  core::mat4 inverse_projection() const { return core::inverse(_projection); }
  // creates a new mat4
  core::mat4 inverse_view() const { return core::inverse(_view); }
  core::vec3 front() const { return _front; }
  core::vec3 right() const { return _right; }
  core::vec3 up() const { return _up; }
  core::vec3 position() const { return _position; }

public:
  float fov{90.0f}; // zoom var ?
  float camera_speed_multiplyer{1};

  float far{1000.f};
  float near{0.1f};

private:
  core::window_t &_window;

  core::vec3 _position{0, 2, 0};

  core::mat4 _projection{1.0f};
  core::mat4 _view{1.0f};

  core::vec3 _front{1, 0, 0};
  core::vec3 _up{0, 1, 0};
  core::vec3 _right{1, 0, 0};

  core::vec2 _initial_mouse{};

  float _yaw{0};
  float _pitch{0};
  float _mouse_speed{0.005f};
  float _mouse_sensitivity{100.f};
};

void editor_camera_t::update_projection(float aspect_ratio) {
  static float s_aspect_ratio = 0;
  if (s_aspect_ratio != aspect_ratio) {
    _projection =
        core::perspective(core::radians(fov), aspect_ratio, near, far);
    s_aspect_ratio = aspect_ratio;
  }
}

void editor_camera_t::update(float ts) {
  auto [width, height] = _window.dimensions();
  update_projection(float(width) / float(height));

  double curX, curY;
  glfwGetCursorPos(_window.window(), &curX, &curY);

  float velocity = _mouse_speed * ts;

  if (glfwGetKey(_window.window(), GLFW_KEY_W))
    _position += _front * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_S))
    _position -= _front * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_D))
    _position += _right * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_A))
    _position -= _right * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_SPACE))
    _position += _up * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_LEFT_SHIFT))
    _position -= _up * velocity;

  core::vec2 mouse{curX, curY};
  core::vec2 difference = mouse - _initial_mouse;
  _initial_mouse = mouse;

  if (glfwGetMouseButton(_window.window(), GLFW_MOUSE_BUTTON_1)) {

    difference.x = difference.x / float(width);
    difference.y = -(difference.y / float(height));

    _yaw += difference.x * _mouse_sensitivity;
    _pitch += difference.y * _mouse_sensitivity;

    if (_pitch > 89.0f)
      _pitch = 89.0f;
    if (_pitch < -89.0f)
      _pitch = -89.0f;
  }

  core::vec3 front;
  front.x = core::cos(core::radians(_yaw)) * core::cos(core::radians(_pitch));
  front.y = core::sin(core::radians(_pitch));
  front.z = core::sin(core::radians(_yaw)) * core::cos(core::radians(_pitch));
  _front = front * camera_speed_multiplyer;
  _right = core::normalize(core::cross(_front, core::vec3{0, 1, 0}));
  _up = core::normalize(core::cross(_right, _front));

  _view = core::lookAt(_position, _position + _front, core::vec3{0, 1, 0});
}

int main() {
  //  core::log_t::set_log_level(core::log_level_t::e_info);

  core::window_t window{"app", 640, 420};
  auto [width, height] = window.dimensions();

  gfx::context_t context{false};

  gfx::handle_sampler_t sampler = context.create_sampler({});

  VkFormat final_image_format = VK_FORMAT_R8G8B8A8_UNORM;
  gfx::config_image_t config_final_image{};
  config_final_image.vk_width = width;
  config_final_image.vk_height = height;
  config_final_image.vk_depth = 1;
  config_final_image.vk_type = VK_IMAGE_TYPE_2D;
  config_final_image.vk_format = final_image_format;
  config_final_image.vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_STORAGE_BIT;
  config_final_image.vk_mips = 1;
  config_final_image.debug_name = "final image";
  gfx::handle_image_t final_image = context.create_image(config_final_image);
  gfx::handle_image_view_t final_image_view =
      context.create_image_view({.handle_image = final_image});

  gfx::base_config_t base_config{.window = window,
                                 .context = context,
                                 .sampler = sampler,
                                 .final_image_view = final_image_view};
  gfx::base_t base{base_config};

  gfx::helper::imgui_init(window, context, base._swapchain, final_image_format);

  // build bvh
  std::vector<core::aabb_t> aabbs{};
  std::vector<core::vec3> centers{};
  std::vector<triangle_t> triangles{};

  //     triangle_t triangle{
  //         core::vec3{-0.5, -0.5, 0},
  //         core::vec3{ 0.5, -0.5, 0},
  //         core::vec3{ 0  ,  0.5, 0},
  //     };
  //     core::aabb_t aabb{};
  //     aabb.grow(triangle.v0).grow(triangle.v1).grow(triangle.v2);
  //     core::vec3 center{};
  //     center = (triangle.v0 + triangle.v1 + triangle.v2) / 3.f;
  //     aabbs.push_back(aabb);
  //     triangles.push_back(triangle);
  //     aabbs.push_back(aabb);

  // core::model_t model =
  //     core::load_model_from_path("../../../horizon_cpy/assets/models/"
  //                                "sponza_bbk/SponzaMerged/SponzaMerged.obj");
  core::model_t model = core::load_model_from_path(
      "../../../horizon_cpy/assets/models/corenl_box.obj");

  for (auto mesh : model.meshes) {
    for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
      triangle_t triangle{
          .v0 = mesh.vertices[mesh.indices[i + 0]].position,
          .v1 = mesh.vertices[mesh.indices[i + 1]].position,
          .v2 = mesh.vertices[mesh.indices[i + 2]].position,
      };
      core::aabb_t aabb{};
      aabb.grow(triangle.v0).grow(triangle.v1).grow(triangle.v2);
      core::vec3 center{};
      center = (triangle.v0 + triangle.v1 + triangle.v2) / 3.f;

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

  renderer::raygen raygen{base};
  renderer::trace trace{base};
  renderer::shade shade{base};
  shade.update_descriptor_set(final_image_view);

  gfx::config_buffer_t config_num_rays_buffer{};
  config_num_rays_buffer.vk_size = sizeof(uint32_t);
  config_num_rays_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  config_num_rays_buffer.vk_buffer_usage_flags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t num_rays_buffer =
      context.create_buffer(config_num_rays_buffer);

  gfx::config_buffer_t config_dispatch_indirect_buffer{};
  config_dispatch_indirect_buffer.vk_size = sizeof(VkDispatchIndirectCommand);
  config_dispatch_indirect_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  config_dispatch_indirect_buffer.vk_buffer_usage_flags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t dispatch_indirect_buffer =
      context.create_buffer(config_dispatch_indirect_buffer);

  gfx::config_buffer_t config_ray_datas_buffer{};
  config_ray_datas_buffer.vk_size = sizeof(ray_data_t) * width * height;
  config_ray_datas_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  config_ray_datas_buffer.vk_buffer_usage_flags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t ray_datas_buffer =
      context.create_buffer(config_ray_datas_buffer);

  gfx::config_buffer_t config_bvh_buffer{};
  config_bvh_buffer.vk_size = sizeof(bvh_t);
  config_bvh_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  config_bvh_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t bvh_buffer = context.create_buffer(config_bvh_buffer);

  gfx::config_buffer_t config_nodes_buffer{};
  config_nodes_buffer.vk_size = sizeof(core::bvh::node_t) * bvh.nodes.size();
  config_nodes_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  config_nodes_buffer.vk_buffer_usage_flags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t nodes_buffer = gfx::helper::create_buffer_staged(
      context, base._command_pool, config_nodes_buffer, bvh.nodes.data(),
      sizeof(core::bvh::node_t) * bvh.nodes.size());

  gfx::config_buffer_t config_primitive_indices_buffer{};
  config_primitive_indices_buffer.vk_size =
      sizeof(uint32_t) * bvh.primitive_indices.size();
  config_primitive_indices_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  config_primitive_indices_buffer.vk_buffer_usage_flags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t primitive_indices_buffer =
      gfx::helper::create_buffer_staged(
          context, base._command_pool, config_primitive_indices_buffer,
          bvh.primitive_indices.data(),
          sizeof(uint32_t) * bvh.primitive_indices.size());

  reinterpret_cast<bvh_t *>(context.map_buffer(bvh_buffer))->nodes =
      context.get_buffer_device_address(nodes_buffer);
  reinterpret_cast<bvh_t *>(context.map_buffer(bvh_buffer))->primitive_indices =
      context.get_buffer_device_address(primitive_indices_buffer);

  gfx::config_buffer_t config_triangles_buffer{};
  config_triangles_buffer.vk_size = sizeof(triangle_t) * triangles.size();
  config_triangles_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  config_triangles_buffer.vk_buffer_usage_flags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t triangles_buffer = gfx::helper::create_buffer_staged(
      context, base._command_pool, config_triangles_buffer, triangles.data(),
      sizeof(triangle_t) * triangles.size());

  gfx::config_buffer_t config_camera_buffer{};
  config_camera_buffer.vk_size = sizeof(camera_t);
  config_camera_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  config_camera_buffer.vk_buffer_usage_flags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_managed_buffer_t camera_buffer = base.create_buffer(
      gfx::resource_update_policy_t::e_every_frame, config_camera_buffer);

  gfx::config_buffer_t config_hits_buffer{};
  config_hits_buffer.vk_size =
      sizeof(hit_t) * width * height * 1.5f; // 1.5f for extra debug info
  config_hits_buffer.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  config_hits_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  gfx::handle_buffer_t hits_buffer = context.create_buffer(config_hits_buffer);

  editor_camera_t editor_camera{window};
  float target_fps = 10000000.f;
  auto last_time = std::chrono::system_clock::now();
  core::frame_timer_t frame_timer{60.f};

  while (!window.should_close()) {
    core::clear_frame_function_times();
    core::window_t::poll_events();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
      break;

    auto current_time = std::chrono::system_clock::now();
    auto time_difference = current_time - last_time;
    if (time_difference.count() / 1e6 < 1000.f / target_fps) {
      continue;
    }
    last_time = current_time;
    auto dt = frame_timer.update();

    editor_camera.update(dt.count());
    // horizon_info("{} {}", core::to_string(editor_camera.position()),
    // core::to_string(editor_camera.front()));

    base.begin();
    auto cbuf = base.current_commandbuffer();

    reinterpret_cast<camera_t *>(context.map_buffer(base.buffer(camera_buffer)))
        ->inverse_projection = editor_camera.inverse_projection();
    reinterpret_cast<camera_t *>(context.map_buffer(base.buffer(camera_buffer)))
        ->inverse_view = editor_camera.inverse_view();

    context.cmd_image_memory_barrier(
        cbuf, final_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // dispatch work

    // TODO: clear image
    renderer::push_constant_t push_constant{};
    push_constant.num_rays = context.get_buffer_device_address(num_rays_buffer);
    push_constant.trace_indirect_cmd =
        context.get_buffer_device_address(dispatch_indirect_buffer);
    push_constant.ray_datas =
        context.get_buffer_device_address(ray_datas_buffer);
    push_constant.bvh = context.get_buffer_device_address(bvh_buffer);
    push_constant.triangles =
        context.get_buffer_device_address(triangles_buffer);
    push_constant.camera =
        context.get_buffer_device_address(base.buffer(camera_buffer));
    push_constant.hits = context.get_buffer_device_address(hits_buffer);
    push_constant.width = width;
    push_constant.height = height;

    raygen.render(cbuf, push_constant);
    context.cmd_buffer_memory_barrier(
        cbuf, ray_datas_buffer,
        context.get_buffer(ray_datas_buffer).config.vk_size, 0,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    trace.render(cbuf, dispatch_indirect_buffer, push_constant);
    context.cmd_buffer_memory_barrier(
        cbuf, hits_buffer, context.get_buffer(hits_buffer).config.vk_size, 0,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    shade.render(cbuf, push_constant);

    // Transition image to color attachment optimal
    context.cmd_image_memory_barrier(
        cbuf, final_image, VK_IMAGE_LAYOUT_GENERAL,
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
    color_rendering_attachment.handle_image_view = final_image_view;

    // begin rendering
    context.cmd_begin_rendering(cbuf, {color_rendering_attachment},
                                std::nullopt,
                                VkRect2D{VkOffset2D{},
                                         {static_cast<uint32_t>(width),
                                          static_cast<uint32_t>(height)}});

    gfx::helper::imgui_newframe();
    ImGui::Begin("test");
    ImGui::End();
    gfx::helper::imgui_endframe(context, cbuf);

    // end rendering
    context.cmd_end_rendering(cbuf);

    // transition image to shader read only optimal
    context.cmd_image_memory_barrier(
        cbuf, final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    base.end();
    raygen.get_time();
    trace.get_time();
    shade.get_time();
  }
  context.wait_idle();

  gfx::helper::imgui_shutdown();

  return 0;
}
