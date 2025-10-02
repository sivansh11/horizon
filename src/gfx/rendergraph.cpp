#include "horizon/gfx/rendergraph.hpp"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include <stdexcept>

namespace gfx {

pass_t::pass_t(std::function<void(handle_commandbuffer_t cmd)> callback)
    : callback(callback) {}

pass_t& pass_t::add_read_buffer(
    handle_buffer_t buffer, VkAccessFlags vk_access,
    VkPipelineStageFlags           vk_pipeline_state,
    const buffer_resource_range_t& buffer_resource_range) {
  resource_t resource{};
  resource.type      = resource_type_t::e_buffer;
  resource.as.buffer = {buffer, vk_access, vk_pipeline_state,
                        buffer_resource_range};
  read_resources.emplace_back(resource);
  return *this;
}

pass_t& pass_t::add_write_buffer(
    handle_buffer_t buffer, VkAccessFlags vk_access,
    VkPipelineStageFlags           vk_pipeline_state,
    const buffer_resource_range_t& buffer_resource_range) {
  resource_t resource{};
  resource.type      = resource_type_t::e_buffer;
  resource.as.buffer = {buffer, vk_access, vk_pipeline_state,
                        buffer_resource_range};
  write_resources.emplace_back(resource);
  return *this;
}

pass_t& pass_t::add_read_image(
    handle_image_t image, VkAccessFlags vk_access,
    VkPipelineStageFlags vk_pipeline_state, VkImageLayout vk_image_layout,
    const image_resource_range_t& image_resource_range) {
  resource_t resource{};
  resource.type     = resource_type_t::e_image;
  resource.as.image = {image, vk_access, vk_pipeline_state, vk_image_layout,
                       image_resource_range};
  read_resources.emplace_back(resource);
  return *this;
}

pass_t& pass_t::add_write_image(
    handle_image_t image, VkAccessFlags vk_access,
    VkPipelineStageFlags vk_pipeline_state, VkImageLayout vk_image_layout,
    const image_resource_range_t& image_resource_range) {
  resource_t resource{};
  resource.type     = resource_type_t::e_image;
  resource.as.image = {image, vk_access, vk_pipeline_state, vk_image_layout,
                       image_resource_range};
  write_resources.emplace_back(resource);
  return *this;
}

pass_t& rendergraph_t::add_pass(
    std::function<void(handle_commandbuffer_t cmd)> callback) {
  pass_t& pass = passes.emplace_back(callback);
  return pass;
}

}  // namespace gfx
