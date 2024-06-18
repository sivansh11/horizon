#include "core/core.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

template <typename T>
std::pair<gfx::handle_buffer_t, T *> create_buffer(gfx::context_t& context) {
    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = sizeof(T);
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;   
    config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    gfx::handle_buffer_t buffer = context.create_buffer(config_buffer);

    return { buffer, reinterpret_cast<T *>(context.map_buffer(buffer)) };
}

int main(int argc, char **argv) {
    core::log_t::set_log_level(core::log_level_t::e_info);

    gfx::context_t context{ true };

    gfx::config_descriptor_set_layout_t config_test_descriptor_set_layout{};
    config_test_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    gfx::handle_descriptor_set_layout_t test_descriptor_set_layout = context.create_descriptor_set_layout(config_test_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_test_pipeline_layout{};
    config_test_pipeline_layout.add_descriptor_set_layout(test_descriptor_set_layout);
    gfx::handle_pipeline_layout_t test_pipeline_layout = context.create_pipeline_layout(config_test_pipeline_layout);

    gfx::config_pipeline_t config_test_pipeline{};
    config_test_pipeline.handle_pipeline_layout = test_pipeline_layout;
    config_test_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../projects/matrix_test/test.slang", .is_code = false, .name = "test", .type = gfx::shader_type_t::e_compute }));
    gfx::handle_pipeline_t test_pipeline = context.create_compute_pipeline(config_test_pipeline);

    gfx::config_buffer_t config_uniform_buffer{};
    config_uniform_buffer.vk_size = 1 * sizeof(VkDeviceAddress);
    config_uniform_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_uniform_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    gfx::handle_buffer_t uniform_buffer = context.create_buffer(config_uniform_buffer);

    auto [result,  p_result]  = create_buffer<glm::mat4>(context);    

    reinterpret_cast<VkDeviceAddress *>(context.map_buffer(uniform_buffer))[0] = context.get_buffer_device_address(result);

    gfx::handle_descriptor_set_t test_descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = test_descriptor_set_layout });
    context.update_descriptor_set(test_descriptor_set)
        .push_buffer_write(0, { .handle_buffer = uniform_buffer, })
        .commit();

    gfx::handle_command_pool_t command_pool = context.create_command_pool({});

    gfx::handle_commandbuffer_t commandbuffer = gfx::helper::start_single_use_commandbuffer(context, command_pool);
    context.cmd_bind_pipeline(commandbuffer, test_pipeline);
    context.cmd_bind_descriptor_sets(commandbuffer, test_pipeline, 0, { test_descriptor_set });
    context.cmd_dispatch(commandbuffer, 1, 1, 1);
    gfx::helper::end_single_use_commandbuffer(context, commandbuffer);

    std::cout << "Expected: " << glm::to_string(glm::translate(glm::mat4{ 1.f }, { 1, 2, 3 })) << '\n';
    std::cout << "Real: " << glm::to_string(*p_result) << '\n';

    return 0;
}
