#ifndef NEW_CONTEXT_HPP
#define NEW_CONTEXT_HPP

#include "horizon/core/core.hpp"

#include "horizon/gfx/types.hpp"

#include <cstdint>
#include <volk.h>
#include <vulkan/vulkan_core.h>
#include "VkBootstrap.h"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VK_NO_PROTOTYPE
#include <vk_mem_alloc.h>

#include <limits>
#include <map>
#include <optional>

namespace core {

class window_t;

} // namespace core

namespace utils {

template <typename handle_t, typename map_t, typename data_t>
inline handle_t create_and_insert_new_handle(map_t &map, data_t &data) {
  handle_t handle = map.size() ? map.size() : 1;
  while (map.contains(handle)) {
    handle++;
  }
  map.insert({handle, data});
  return handle;
}

template <typename data_t, typename handle_t, typename map_t>
inline data_t &assert_and_get_data(handle_t handle, map_t &map) {
  assert(map.contains(handle));
  return map[handle];
}

} // namespace utils

namespace gfx {

constexpr VmaMemoryUsage default_vma_memory_usage =
    VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
struct config_buffer_t {
  VkDeviceSize vk_size;
  VkBufferUsageFlags vk_buffer_usage_flags;
  VmaAllocationCreateFlags vma_allocation_create_flags;
  VmaMemoryUsage vma_memory_usage = default_vma_memory_usage;
  std::string debug_name = "";
};

struct config_sampler_t {
  VkFilter vk_mag_filter = /*VK_FILTER_LINEAR*/ VK_FILTER_NEAREST;
  VkFilter vk_min_filter = /*VK_FILTER_LINEAR*/ VK_FILTER_NEAREST;
  VkSamplerMipmapMode vk_mipmap_mode =
      /*VK_SAMPLER_MIPMAP_MODE_LINEAR*/ VK_SAMPLER_MIPMAP_MODE_NEAREST;
  VkSamplerAddressMode vk_address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode vk_address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode vk_address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  float vk_mip_lod_bias = 0;
  VkBool32 vk_anisotropy_enable = VK_FALSE;
  float vk_max_anisotropy = 0; // maybe shouldnt be 0 ?
  VkBool32 vk_compare_enable = VK_FALSE;
  VkCompareOp vk_compare_op =
      VK_COMPARE_OP_LESS; // maybe should choose a different default
  float vk_min_lod = 0;
  float vk_max_lod = VK_LOD_CLAMP_NONE;
  VkBorderColor vk_border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  VkBool32 vk_unnormalized_coordinates = VK_FALSE;
  std::string debug_name = "";
};

constexpr const uint32_t vk_auto_calculate_mip_levels =
    std::numeric_limits<uint32_t>::max();
struct config_image_t {
  uint32_t vk_width;
  uint32_t vk_height;
  uint32_t vk_depth;
  VkImageType vk_type;
  VkFormat vk_format;
  VkImageUsageFlags vk_usage;
  VmaAllocationCreateFlags vma_allocation_create_flags;
  uint32_t vk_mips = vk_auto_calculate_mip_levels;
  VkSampleCountFlagBits vk_sample_count = VK_SAMPLE_COUNT_1_BIT;
  uint32_t vk_array_layers = 1;
  VkImageTiling vk_tiling = VK_IMAGE_TILING_OPTIMAL;
  VkImageLayout vk_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VmaMemoryUsage vma_memory_usage = VMA_MEMORY_USAGE_AUTO;
  std::string debug_name = "";
};

constexpr VkImageViewType vk_auto_image_view_type =
    static_cast<VkImageViewType>(0x7FFFFFFF -
                                 1); // -1 cause I dont wanna collide with the
                                     // max enum enum in the enum definition
constexpr VkFormat vk_auto_image_format = static_cast<VkFormat>(0x7FFFFFFF - 1);
constexpr uint32_t vk_auto_mips = 0x7FFFFFFF;   // this should work I guess ??
constexpr uint32_t vk_auto_layers = 0x7FFFFFFF; // this should work I guess ??
constexpr uint32_t vk_default_base_mip_level = 0;
constexpr uint32_t vk_default_base_array_layer = 0;
struct config_image_view_t {
  handle_image_t handle_image = core::null_handle;
  VkImageViewType vk_image_view_type = vk_auto_image_view_type;
  VkFormat vk_format = vk_auto_image_format;
  uint32_t vk_base_mip_level = vk_default_base_mip_level;
  uint32_t vk_mips =
      vk_auto_mips; // gets the max level count possible for the image
  uint32_t vk_base_array_layer = vk_default_base_array_layer;
  uint32_t vk_layers = vk_auto_layers;
  std::string debug_name = "";
};

struct config_descriptor_set_layout_t {
  config_descriptor_set_layout_t &
  add_layout_binding(uint32_t vk_binding, VkDescriptorType vk_descriptor_type,
                     VkShaderStageFlags vk_shader_stages,
                     uint32_t vk_count = 1);

  std::vector<VkDescriptorSetLayoutBinding> vk_descriptor_set_layout_bindings;
  bool use_bindless = true;
  std::string debug_name = "";
};

struct config_descriptor_set_t {
  handle_descriptor_set_layout_t handle_descriptor_set_layout =
      core::null_handle;
  std::string debug_name = "";
};

enum class shader_type_t {
  e_vertex,
  e_fragment,
  e_compute,
};

enum class shader_language_t {
  e_unknown,
  e_slang,
  e_glsl,
};

struct config_pipeline_layout_t {
  config_pipeline_layout_t &
  add_descriptor_set_layout(handle_descriptor_set_layout_t handle);
  config_pipeline_layout_t &
  add_push_constant(uint32_t vk_size, VkShaderStageFlagBits vk_shader_stages,
                    uint32_t vk_offset = 0);

  std::vector<handle_descriptor_set_layout_t> handle_descriptor_set_layouts{};
  std::vector<VkPushConstantRange> vk_push_constant_ranges{};
  std::string debug_name = "";
};

struct config_shader_t {
  std::string code_or_path;
  bool is_code = false;
  std::string name;
  shader_type_t type;
  shader_language_t language = shader_language_t::e_slang;
  std::string debug_name = "";
};

VkPipelineColorBlendAttachmentState default_color_blend_attachment();

struct config_pipeline_t {
  config_pipeline_t();

  config_pipeline_t &add_shader(handle_shader_t handle);

  config_pipeline_t &add_color_attachment(
      VkFormat vk_format,
      const VkPipelineColorBlendAttachmentState &vk_pipeline_color_blend_state);
  config_pipeline_t &set_depth_attachment(
      VkFormat vk_format,
      const VkPipelineDepthStencilStateCreateInfo &vk_pipeline_depth_state);

  config_pipeline_t &add_dynamic_state(const VkDynamicState &vk_dynamic_state);

  config_pipeline_t &
  add_vertex_input_binding_description(uint32_t vk_binding, uint32_t vk_stride,
                                       VkVertexInputRate vk_input_rate);
  config_pipeline_t &add_vertex_input_attribute_description(
      uint32_t vk_binding, uint32_t vk_location, VkFormat vk_format,
      uint32_t vk_offset);

  config_pipeline_t &
  set_pipeline_input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo
                                        &vk_pipeline_input_assembly_state);
  config_pipeline_t &
  set_pipeline_rasterization_state(const VkPipelineRasterizationStateCreateInfo
                                       &vk_pipeline_rasterization_state);
  config_pipeline_t &
  set_pipeline_multisample_state(const VkPipelineMultisampleStateCreateInfo
                                     &vk_pipeline_multisample_state);

  handle_pipeline_layout_t handle_pipeline_layout = core::null_handle;
  std::vector<handle_shader_t> handle_shaders{};
  std::vector<VkFormat> vk_color_formats{};
  VkFormat vk_depth_format{};
  std::vector<VkPipelineColorBlendAttachmentState>
      vk_pipeline_color_blend_attachment_states{};
  VkPipelineDepthStencilStateCreateInfo
      vk_pipeline_depth_stencil_state_create_info{};
  std::vector<VkDynamicState> vk_dynamic_states{};
  std::vector<VkVertexInputAttributeDescription>
      vk_vertex_input_attribute_descriptions{};
  std::vector<VkVertexInputBindingDescription>
      vk_vertex_input_binding_descriptions{};
  VkPipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state{};
  VkPipelineRasterizationStateCreateInfo vk_pipeline_rasterization_state{};
  VkPipelineMultisampleStateCreateInfo vk_pipeline_multisample_state{};
  std::string debug_name = "";
};

struct config_fence_t {
  std::string debug_name = "";
};

struct config_semaphore_t {
  std::string debug_name = "";
};

struct config_command_pool_t {
  std::string debug_name = "";
};

struct config_commandbuffer_t {
  handle_command_pool_t handle_command_pool;
  std::string debug_name = "";
};

struct config_timer_t {
  std::string debug_name = "";
};

namespace internal {

struct queue_t {
  VkQueue vk_queue;
  uint32_t vk_index;
  operator VkQueue() { return vk_queue; }
};

struct swapchain_t {
  vkb::Swapchain vk_swapchain;
  VkSurfaceKHR vk_surface;
  std::vector<handle_image_t> handle_images;
  std::vector<handle_image_view_t> handle_image_views;
  operator vkb::Swapchain() { return vk_swapchain; }
};

struct buffer_t {
  VkBuffer vk_buffer;
  VmaAllocation vma_allocation;
  config_buffer_t config;
  void *p_data = nullptr;
  VkDeviceAddress vk_device_address;
  operator VkBuffer() { return vk_buffer; }
};

struct sampler_t {
  VkSampler vk_sampler;
  config_sampler_t config;
  operator VkSampler() { return vk_sampler; }
};

struct image_t {
  VkImage vk_image;
  VmaAllocation vma_allocation;
  config_image_t config;
  void *p_data = nullptr;
  bool from_swapchain = false;
  operator VkImage() { return vk_image; }
};

struct image_view_t {
  VkImageView vk_image_view;
  config_image_view_t config;
  operator VkImageView() { return vk_image_view; }
};

struct descriptor_set_layout_t {
  VkDescriptorSetLayout vk_descriptor_set_layout;
  config_descriptor_set_layout_t config;
  operator VkDescriptorSetLayout() { return vk_descriptor_set_layout; }
};

struct descriptor_set_t {
  VkDescriptorSet vk_descriptor_set;
  config_descriptor_set_t config;
  operator VkDescriptorSet() { return vk_descriptor_set; }
};

struct pipeline_layout_t {
  VkPipelineLayout vk_pipeline_layout;
  config_pipeline_layout_t config;
  operator VkPipelineLayout() { return vk_pipeline_layout; }
};

struct shader_t {
  VkShaderModule vk_shader;
  config_shader_t config;
  operator VkShaderModule() { return vk_shader; }
};

struct pipeline_t {
  VkPipeline vk_pipeline;
  VkPipelineBindPoint vk_pipeline_bind_point;
  config_pipeline_t config;
  operator VkPipeline() { return vk_pipeline; }
};

struct fence_t {
  VkFence vk_fence;
  config_fence_t config;
  operator VkFence() { return vk_fence; }
};

struct semaphore_t {
  VkSemaphore vk_semaphore;
  config_semaphore_t config;
  operator VkSemaphore() { return vk_semaphore; }
};

struct command_pool_t {
  VkCommandPool vk_command_pool;
  config_command_pool_t config;
  operator VkCommandPool() { return vk_command_pool; }
};

struct commandbuffer_t {
  VkCommandBuffer vk_commandbuffer;
  config_commandbuffer_t config;
  operator VkCommandBuffer() { return vk_commandbuffer; }
};

struct timer_t {
  VkQueryPool vk_query_pool;
  config_timer_t config;
  operator VkQueryPool() { return vk_query_pool; }
};

} // namespace internal

class context_t;

struct buffer_descriptor_info_t {
  handle_buffer_t handle_buffer = core::null_handle;
  VkDeviceSize vk_offset = 0;
  VkDeviceSize vk_range = VK_WHOLE_SIZE;
};

struct image_descriptor_info_t {
  handle_sampler_t handle_sampler = core::null_handle;
  handle_image_view_t handle_image_view = core::null_handle;
  VkImageLayout vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct update_descriptor_set_t {
  update_descriptor_set_t &
  push_buffer_write(uint32_t binding, const buffer_descriptor_info_t &info,
                    uint32_t array_element = 0);
  update_descriptor_set_t &push_image_write(uint32_t binding,
                                            const image_descriptor_info_t &info,
                                            uint32_t array_element = 0);
  void commit();

  context_t &context;
  handle_descriptor_set_t handle = core::null_handle;
  std::vector<VkWriteDescriptorSet> vk_writes;
};

struct rendering_attachment_t {
  handle_image_view_t handle_image_view = core::null_handle;
  VkImageLayout image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
  VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
  VkClearValue clear_value = {0, 0, 0, 0};
};

struct buffer_copy_info_t {
  VkDeviceSize vk_src_offset = 0;
  VkDeviceSize vk_dst_offset = 0;
  VkDeviceSize vk_size = VK_WHOLE_SIZE;
};

class context_t {
public:
  context_t(const bool enable_validation);
  ~context_t();

  void wait_idle();

  handle_swapchain_t create_swapchain(const core::window_t &window);
  void destroy_swapchain(handle_swapchain_t handle);
  std::vector<handle_image_t> get_swapchain_images(handle_swapchain_t handle);
  std::vector<handle_image_view_t>
  get_swapchain_image_views(handle_swapchain_t handle);
  std::optional<uint32_t>
  get_swapchain_next_image_index(handle_swapchain_t handle,
                                 handle_semaphore_t handle_swapchain,
                                 handle_fence_t handle_fence);
  bool present_swapchain(handle_swapchain_t handle, uint32_t image_index,
                         std::vector<handle_semaphore_t> handle_semaphore);
  internal::swapchain_t &get_swapchain(handle_swapchain_t handle);

  handle_buffer_t create_buffer(const config_buffer_t &config);
  void destroy_buffer(handle_buffer_t handle);
  void *map_buffer(handle_buffer_t handle);
  void unmap_buffer(handle_buffer_t handle);
  VkDeviceAddress get_buffer_device_address(handle_buffer_t handle);
  internal::buffer_t &get_buffer(handle_buffer_t handle);
  void flush_buffer(handle_buffer_t handle);
  // This iterates over all active handles and returns the final size, shouldnt
  // be used in performance critical paths
  uint64_t get_total_buffer_memory_allocated();

  handle_sampler_t create_sampler(const config_sampler_t &config);
  void destroy_sampler(handle_sampler_t handle);
  internal::sampler_t &get_sampler(handle_sampler_t handle);

  handle_image_t create_image(const config_image_t &config);
  void destroy_image(handle_image_t handle);
  void *map_image(handle_image_t handle);
  void unmap_image(handle_image_t handle);
  internal::image_t &get_image(handle_image_t handle);
  void flush_image(handle_image_t handle);

  handle_image_view_t create_image_view(const config_image_view_t &config);
  void destroy_image_view(handle_image_view_t handle);
  internal::image_view_t &get_image_view(handle_image_view_t handle);

  handle_descriptor_set_layout_t
  create_descriptor_set_layout(const config_descriptor_set_layout_t &config);
  void destroy_descriptor_set_layout(handle_descriptor_set_layout_t handle);
  internal::descriptor_set_layout_t &
  get_descriptor_set_layout(handle_descriptor_set_layout_t handle);

  handle_descriptor_set_t
  allocate_descriptor_set(const config_descriptor_set_t &config);
  void free_descriptor_set(handle_descriptor_set_t handle);
  update_descriptor_set_t update_descriptor_set(handle_descriptor_set_t handle);
  internal::descriptor_set_t &
  get_descriptor_set(handle_descriptor_set_t handle);

  handle_pipeline_layout_t
  create_pipeline_layout(const config_pipeline_layout_t &config);
  void destroy_pipeline_layout(handle_pipeline_layout_t handle);
  internal::pipeline_layout_t &
  get_pipeline_layout(handle_pipeline_layout_t handle);

  handle_shader_t create_shader(const config_shader_t &config);
  void destroy_shader(handle_shader_t handle);
  internal::shader_t &get_shader(handle_shader_t handle);

  handle_pipeline_t create_compute_pipeline(const config_pipeline_t &config);
  handle_pipeline_t create_graphics_pipeline(const config_pipeline_t &config);
  void destroy_pipeline(handle_pipeline_t handle);
  internal::pipeline_t &get_pipeline(handle_pipeline_t handle);

  handle_fence_t create_fence(const config_fence_t &config);
  void destroy_fence(handle_fence_t handle);
  void wait_fence(handle_fence_t handle);
  void reset_fence(handle_fence_t handle);
  internal::fence_t &get_fence(handle_fence_t handle);

  handle_semaphore_t create_semaphore(const config_semaphore_t &config);
  void destroy_semaphore(handle_semaphore_t handle);
  internal::semaphore_t &get_semaphore(handle_semaphore_t handle);

  handle_command_pool_t
  create_command_pool(const config_command_pool_t &config);
  void destroy_command_pool(handle_command_pool_t handle);
  internal::command_pool_t &get_command_pool(handle_command_pool_t handle);

  handle_commandbuffer_t
  allocate_commandbuffer(const config_commandbuffer_t &config);
  void free_commandbuffer(handle_commandbuffer_t handle);
  void begin_commandbuffer(handle_commandbuffer_t handle,
                           bool single_use = false);
  void end_commandbuffer(handle_commandbuffer_t handle);
  void submit_commandbuffer(
      handle_commandbuffer_t handle,
      const std::vector<handle_semaphore_t> &wait_semaphore_handles,
      const std::vector<VkPipelineStageFlags> &vk_pipeline_stages,
      const std::vector<handle_semaphore_t> &signal_semaphore_handles,
      handle_fence_t handle_fence);
  internal::commandbuffer_t &get_commandbuffer(handle_commandbuffer_t handle);

  handle_timer_t create_timer(const config_timer_t &config);
  void destroy_timer(handle_timer_t handle);
  std::optional<float> timer_get_time(handle_timer_t handle);
  internal::timer_t &get_timer(handle_timer_t handle);

  void cmd_bind_pipeline(handle_commandbuffer_t handle_commandbuffer,
                         handle_pipeline_t handle_pipeline);
  void cmd_bind_descriptor_sets(
      handle_commandbuffer_t handle_commandbuffer,
      handle_pipeline_t handle_pipeline, uint32_t vk_first_set,
      const std::vector<handle_descriptor_set_t> &handle_descriptor_sets);
  void cmd_push_constants(handle_commandbuffer_t handle_commandbuffer,
                          handle_pipeline_t handle_pipeline,
                          VkShaderStageFlags vk_shader_stages,
                          uint32_t vk_offset, uint32_t vk_size,
                          const void *vk_data);
  void cmd_dispatch(handle_commandbuffer_t handle_commandbuffer,
                    uint32_t vk_group_count_x, uint32_t vk_group_count_y,
                    uint32_t vk_group_count_z);
  void cmd_dispatch_indirect(handle_commandbuffer_t handle_commandbuffer,
                             handle_buffer_t handle_buffer, uint32_t offset);
  void cmd_set_viewport_and_scissor(handle_commandbuffer_t handle_commandbuffer,
                                    VkViewport vk_viewport,
                                    VkRect2D vk_scissor);
  void cmd_begin_rendering(
      handle_commandbuffer_t handle_commandbuffer,
      const std::vector<rendering_attachment_t> &color_rendering_attachments,
      const std::optional<rendering_attachment_t> &depth_rendering_attachment,
      const VkRect2D &vk_render_area, uint32_t vk_layer_count = 1);
  void cmd_end_rendering(handle_commandbuffer_t handle_commandbuffer);
  void cmd_draw(handle_commandbuffer_t handle_commandbuffer,
                uint32_t vk_vertex_count, uint32_t vk_instance_count,
                uint32_t vk_first_vertex, uint32_t vk_first_instance);
  void cmd_draw_indexed(handle_commandbuffer_t handle_commandbuffer,
                        uint32_t vk_index_count, uint32_t vk_instance_count,
                        uint32_t vk_first_index, int32_t vk_vertex_offset,
                        uint32_t vk_first_instance);
  void cmd_blit_image(handle_commandbuffer_t handle_commandbuffer,
                      handle_image_t src_image_handle,
                      VkImageLayout vk_src_image_layout,
                      handle_image_t dst_image_handle,
                      VkImageLayout vk_dst_image_layout,
                      const std::vector<VkImageBlit> &vk_image_blits,
                      VkFilter vk_filter);
  void cmd_pipeline_barrier(
      handle_commandbuffer_t handle_commandbuffer,
      VkPipelineStageFlags vk_src_pipeline_stage_flags,
      VkPipelineStageFlags vk_dst_pipeline_stage_flags,
      VkDependencyFlags vk_dependency_flags,
      const std::vector<VkMemoryBarrier> &vk_memory_barriers,
      const std::vector<VkBufferMemoryBarrier> &vk_buffer_memory_barriers,
      const std::vector<VkImageMemoryBarrier> &vk_image_memory_barriers);
  // TODO: add mip levels option to this
  void cmd_image_memory_barrier(handle_commandbuffer_t handle_commandbuffer,
                                handle_image_t handle_image,
                                VkImageLayout vk_old_image_layout,
                                VkImageLayout vk_new_image_layout,
                                VkAccessFlags vk_src_access_mask,
                                VkAccessFlags vk_dst_access_mask,
                                VkPipelineStageFlags vk_src_pipeline_stage,
                                VkPipelineStageFlags vk_dst_pipeline_stage);
  void cmd_buffer_memory_barrier(handle_commandbuffer_t handle_commandbuffer,
                                 handle_buffer_t handle_buffer,
                                 VkDeviceSize vk_size, VkDeviceSize vk_offset,
                                 VkAccessFlags vk_src_access_mask,
                                 VkAccessFlags vk_dst_access_mask,
                                 VkPipelineStageFlags vk_src_pipeline_stage,
                                 VkPipelineStageFlags vk_dst_pipeline_stage);
  // maybe expose multiple sub regions
  void cmd_copy_buffer(handle_commandbuffer_t handle_commandbuffer,
                       handle_buffer_t src_handle, handle_buffer_t dst_handle,
                       const buffer_copy_info_t &buffer_copy_info);
  // maybe expose multiple sub regions
  void cmd_copy_buffer_to_image(handle_commandbuffer_t handle_commandbuffer,
                                handle_buffer_t src_buffer,
                                handle_image_t dst_image,
                                VkImageLayout vk_dst_image_layout,
                                const VkBufferImageCopy &vk_buffer_image_copy);
  void
  cmd_bind_vertex_buffers(handle_commandbuffer_t handle_commandbuffer,
                          uint32_t first_binding,
                          const std::vector<handle_buffer_t> &handle_buffers,
                          std::vector<VkDeviceSize> vk_offsets);
  void cmd_bind_index_buffer(handle_commandbuffer_t handle_commandbuffer,
                             handle_buffer_t handle_buffer,
                             VkDeviceSize vk_offset, VkIndexType vk_index_type);
  // maybe expose the reset query pool and write time step functions
  void cmd_begin_timer(handle_commandbuffer_t handle_commandbuffer,
                       handle_timer_t handle,
                       VkPipelineStageFlagBits vk_pipeline_stage_flags =
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
  void cmd_end_timer(handle_commandbuffer_t handle_commandbuffer,
                     handle_timer_t handle,
                     VkPipelineStageFlagBits vk_pipeline_stage_flags =
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  friend struct update_descriptor_set_t;

  vkb::Instance &instance();
  vkb::PhysicalDevice &physical_device();
  vkb::Device &device();
  internal::queue_t &graphics_queue();
  internal::queue_t &present_queue();
  VkDescriptorPool &descriptor_pool();

private:
  void create_instance();
  void create_device();
  void create_allocator();
  void create_descriptor_pool();

private:
  const bool _validation;
  vkb::Instance _vkb_instance;
  vkb::PhysicalDevice _vkb_physical_device;
  vkb::Device _vkb_device;
  internal::queue_t _graphics_queue;
  internal::queue_t _present_queue;
  VmaAllocator _vma_allocator;
  VkDescriptorPool _vk_descriptor_pool;

  std::map<handle_swapchain_t, internal::swapchain_t> _swapchains;
  std::map<handle_buffer_t, internal::buffer_t> _buffers;
  std::map<handle_sampler_t, internal::sampler_t> _samplers;
  std::map<handle_image_t, internal::image_t> _images;
  std::map<handle_image_view_t, internal::image_view_t> _image_views;
  std::map<handle_descriptor_set_layout_t, internal::descriptor_set_layout_t>
      _descriptor_set_layouts;
  std::map<handle_descriptor_set_t, internal::descriptor_set_t>
      _descriptor_sets;
  std::map<handle_pipeline_layout_t, internal::pipeline_layout_t>
      _pipeline_layouts;
  std::map<handle_shader_t, internal::shader_t> _shaders;
  std::map<handle_pipeline_t, internal::pipeline_t> _pipelines;
  std::map<handle_fence_t, internal::fence_t> _fences;
  std::map<handle_semaphore_t, internal::semaphore_t> _semaphores;
  std::map<handle_command_pool_t, internal::command_pool_t> _command_pools;
  std::map<handle_commandbuffer_t, internal::commandbuffer_t> _commandbuffers;
  std::map<handle_timer_t, internal::timer_t> _timers;
};

template <typename T> T to(VkDeviceAddress addr) {
  return reinterpret_cast<T>(addr);
}

} // namespace gfx

#define define_fmt(name)                                                       \
  template <> struct std::formatter<name> {                                    \
    template <typename parse_context_t>                                        \
    constexpr auto parse(parse_context_t &ctx) {                               \
      return ctx.begin();                                                      \
    }                                                                          \
    template <typename format_context_t>                                       \
    auto format(const name &handle, format_context_t &ctx) const {             \
      return std::format_to(ctx.out(), "{}", handle.val);                      \
    }                                                                          \
  }

define_fmt(gfx::handle_swapchain_t);
define_fmt(gfx::handle_buffer_t);
define_fmt(gfx::handle_sampler_t);
define_fmt(gfx::handle_image_t);
define_fmt(gfx::handle_image_view_t);
define_fmt(gfx::handle_descriptor_set_layout_t);
define_fmt(gfx::handle_descriptor_set_t);
define_fmt(gfx::handle_pipeline_layout_t);
define_fmt(gfx::handle_shader_t);
define_fmt(gfx::handle_pipeline_t);
define_fmt(gfx::handle_fence_t);
define_fmt(gfx::handle_semaphore_t);
define_fmt(gfx::handle_command_pool_t);
define_fmt(gfx::handle_commandbuffer_t);
define_fmt(gfx::handle_timer_t);

#define define_handle_hash(name)                                               \
  template <> struct std::hash<name> {                                         \
    size_t operator()(const name &handle) const {                              \
      uint64_t seed = 0;                                                         \
      core::hash_combine(seed, handle.val);                                    \
      return seed;                                                             \
    }                                                                          \
  }

define_handle_hash(gfx::handle_swapchain_t);
define_handle_hash(gfx::handle_buffer_t);
define_handle_hash(gfx::handle_sampler_t);
define_handle_hash(gfx::handle_image_t);
define_handle_hash(gfx::handle_image_view_t);
define_handle_hash(gfx::handle_descriptor_set_layout_t);
define_handle_hash(gfx::handle_descriptor_set_t);
define_handle_hash(gfx::handle_pipeline_layout_t);
define_handle_hash(gfx::handle_shader_t);
define_handle_hash(gfx::handle_pipeline_t);
define_handle_hash(gfx::handle_fence_t);
define_handle_hash(gfx::handle_semaphore_t);
define_handle_hash(gfx::handle_command_pool_t);
define_handle_hash(gfx::handle_commandbuffer_t);
define_handle_hash(gfx::handle_timer_t);

#endif
