#ifndef GFX_RENDERGRAPH_HPP
#define GFX_RENDERGRAPH_HPP

#include "horizon/gfx/context.hpp"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <functional>
#include <vector>

#include "horizon/gfx/types.hpp"

namespace gfx {

enum class resource_type_t : uint8_t {
  e_buffer,
  e_image,
};

struct buffer_resource_t {
  handle_buffer_t         buffer;
  VkAccessFlags           vk_access;
  VkPipelineStageFlags    vk_pipeline_state;
  buffer_resource_range_t buffer_resource_range;
};

struct image_resource_t {
  handle_image_t         image;
  VkAccessFlags          vk_access;
  VkPipelineStageFlags   vk_pipeline_state;
  VkImageLayout          vk_image_layout;
  image_resource_range_t image_resource_range;
};

struct resource_t {
  resource_type_t type;
  union as_t {
    buffer_resource_t buffer;
    image_resource_t  image;
  } as;
  bool operator==(const resource_t& other) const;
};

struct buffer_state_t {
  VkAccessFlags        vk_access;
  VkPipelineStageFlags vk_pipeline_state;
};

struct image_state_t {
  VkAccessFlags        vk_access;
  VkPipelineStageFlags vk_pipeline_state;
  VkImageLayout        vk_image_layout;
};

struct resource_state_t {
  resource_type_t type;
  union as_t {
    buffer_state_t buffer_state;
    image_state_t  image_state;
  } as;
};

/*
 * NOTE: do not put in an external read resource in pass
 * an example of an external resource would be diffusemaps/normalmaps etc
 * these do not need to be synchronised by the rendergraph since they are
 * already in shader_read_only_layout
 */
struct pass_t {
  pass_t(std::function<void(handle_commandbuffer_t cmd)> callback);
  pass_t& add_read_buffer(
      handle_buffer_t buffer, VkAccessFlags vk_access,
      VkPipelineStageFlags           vk_pipeline_state,
      const buffer_resource_range_t& buffer_resource_range = {});
  pass_t& add_write_buffer(
      handle_buffer_t buffer, VkAccessFlags vk_access,
      VkPipelineStageFlags           vk_pipeline_state,
      const buffer_resource_range_t& buffer_resource_range = {});
  pass_t& add_read_image(
      handle_image_t image, VkAccessFlags vk_access,
      VkPipelineStageFlags vk_pipeline_state, VkImageLayout vk_image_layout,
      const image_resource_range_t& image_resource_range = {});
  pass_t& add_write_image(
      handle_image_t image, VkAccessFlags vk_access,
      VkPipelineStageFlags vk_pipeline_state, VkImageLayout vk_image_layout,
      const image_resource_range_t& image_resource_range = {});

  std::function<void(handle_commandbuffer_t cmd)> callback;
  std::vector<resource_t>                         read_resources;
  std::vector<resource_t>                         write_resources;
};

struct rendergraph_t {
  pass_t& add_pass(std::function<void(handle_commandbuffer_t cmd)> callback);
  std::vector<pass_t> passes;
};

}  // namespace gfx

#endif
