#include "core/core.hpp"
#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/bindless_manager.hpp"

struct input_t {
    uint32_t i;
    uint32_t j;
};

struct output_t {
    uint32_t i;
    uint32_t j;
};

int main() {
    gfx::context_t context{ true };

    gfx::bindless_manager_t bindless_manager{ context };

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_descriptor_set_layout(bindless_manager.dsl());
    gfx::handle_pipeline_layout_t pl = context.create_pipeline_layout(cpl);

    gfx::config_pipeline_t cp{};
    cp.handle_pipeline_layout = pl;
    cp.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/bindless_manager/test.slang", .is_code = false, .name = "test", .type = gfx::shader_type_t::e_compute, .language = gfx::shader_language_t::e_slang }));
    gfx::handle_pipeline_t p = context.create_compute_pipeline(cp);

    gfx::config_buffer_t config_input{};
    config_input.vk_size = sizeof(input_t);
    config_input.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_input.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    gfx::handle_buffer_t input_buffer = context.create_buffer(config_input);

    gfx::config_buffer_t config_output{};
    config_output.vk_size = sizeof(output_t);
    config_output.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_output.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    gfx::handle_buffer_t output_buffer = context.create_buffer(config_output);

    bindless_manager.slot(input_buffer, 0);
    bindless_manager.slot(output_buffer, 1);

    gfx::handle_command_pool_t command_pool = context.create_command_pool({});

    auto image = gfx::helper::load_image_from_path(context, command_pool, "../../assets/texture/noise.jpg", VK_FORMAT_R8G8B8A8_SRGB);
    auto image_view = context.create_image_view({.handle_image = image});
    auto sampler = context.create_sampler({});
    bindless_manager.slot(image_view);
    bindless_manager.slot(sampler);

    bindless_manager.flush_updates();

    input_t *input = reinterpret_cast<input_t *>(context.map_buffer(input_buffer));
    input->i = 4;
    input->j = 6;


    gfx::handle_commandbuffer_t cbuff = gfx::helper::start_single_use_commandbuffer(context, command_pool);

    context.cmd_bind_pipeline(cbuff, p);
    context.cmd_bind_descriptor_sets(cbuff, p, 0, { bindless_manager.ds() });
    context.cmd_dispatch(cbuff, 1, 1, 1);

    gfx::helper::end_single_use_commandbuffer(context, cbuff);

    output_t *output = reinterpret_cast<output_t *>(context.map_buffer(output_buffer));

    horizon_info("{} {}\n{} {}", uint32_t(input->i), uint32_t(input->j), uint32_t(output->i), uint32_t(output->j));

    return 0;
}