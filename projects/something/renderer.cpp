#include "renderer.hpp"

#include "gfx/base_renderer.hpp"

#include "rendergraph.hpp"

renderer_t::renderer_t(core::window_t& window) : window(window), base_renderer( window ) {
    auto [width, height] = window.dimensions();
    gfx::config_descriptor_set_layout_t single_config_combined_image_sampler_descriptor_set_layout{};
    single_config_combined_image_sampler_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    single_combined_image_sampler_descriptor_set_layout = base_renderer.context.create_descriptor_set_layout(single_config_combined_image_sampler_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_swapchain_pipeline_layout{};
    config_swapchain_pipeline_layout.add_descriptor_set_layout(single_combined_image_sampler_descriptor_set_layout);
    swapchain_pipeline_layout = base_renderer.context.create_pipeline_layout(config_swapchain_pipeline_layout);

    gfx::config_pipeline_t config_swapchain_pipeline{};
    config_swapchain_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/swapchain/present_to_swapchain.vert").data(), .name = "vertex_shader", .type = gfx::shader_type_t::e_vertex }));
    config_swapchain_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/swapchain/present_to_swapchain.frag").data(), .name = "fragment_shader", .type = gfx::shader_type_t::e_fragment }));
    config_swapchain_pipeline.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    config_swapchain_pipeline.handle_pipeline_layout = swapchain_pipeline_layout;
    swapchain_pipeline = base_renderer.context.create_graphics_pipeline(config_swapchain_pipeline);
    swapchain_descriptor_set = base_renderer.allocate_descriptor_set(renderer::resource_policy_t::e_sparse, { .handle_descriptor_set_layout = single_combined_image_sampler_descriptor_set_layout });

    gfx::config_image_t config_target_image{};
    config_target_image.vk_width = width;
    config_target_image.vk_height = height;
    config_target_image.vk_depth = 1;
    config_target_image.vk_type = VK_IMAGE_TYPE_2D;
    config_target_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    config_target_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_target_image.vk_mips = 1;
    target_image = base_renderer.context.create_image(config_target_image);
    target_image_view = base_renderer.context.create_image_view({ .handle_image = target_image });

    sampler = base_renderer.context.create_sampler({});

    base_renderer.update_descriptor_set(swapchain_descriptor_set)
        .push_image_write(0, renderer::image_descriptor_info_t{ .handle_sampler = sampler, .handle_image_view = target_image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL })
        .commit();

    gfx::config_descriptor_set_layout_t config_dsl{};
    config_dsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
    auto dsl = base_renderer.context.create_descriptor_set_layout(config_dsl);

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(dsl)
                        //   .add_descriptor_set_layout(dsl)
                            .add_descriptor_set_layout(dsl);
    auto pipeline_layout = base_renderer.context.create_pipeline_layout(config_pipeline_layout);
    
    gfx::config_pipeline_t config_pipeline{};
    config_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/directional_light/glsl.vert").data(), .name = "vertex_shader", .type = gfx::shader_type_t::e_vertex }));
    config_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/directional_light/glsl.frag").data(), .name = "fragment_shader", .type = gfx::shader_type_t::e_fragment }));
    config_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
    config_pipeline.add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    config_pipeline.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position));
    config_pipeline.add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, normal));
    config_pipeline.add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv));
    config_pipeline.add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, tangent));
    config_pipeline.add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, bi_tangent));
    config_pipeline.handle_pipeline_layout = pipeline_layout;
    pipeline = base_renderer.context.create_graphics_pipeline(config_pipeline);
    camera_descriptor_set = base_renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, {.handle_descriptor_set_layout = dsl});
    additional_descriptor_set = base_renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, {.handle_descriptor_set_layout = dsl});

    gfx::config_buffer_t config_camera_buffer{};
    config_camera_buffer.vk_size = sizeof(camera_uniform_t);
    config_camera_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_camera_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    camera_buffer = base_renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_camera_buffer);

    gfx::config_buffer_t config_additional_buffer{};
    config_additional_buffer.vk_size = sizeof(additional_uniform_t);
    config_additional_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_additional_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    additional_buffer = base_renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_additional_buffer);

    base_renderer.update_descriptor_set(camera_descriptor_set)
        .push_buffer_write(0, renderer::buffer_descriptor_info_t{.handle_buffer=camera_buffer})
        .commit();
    base_renderer.update_descriptor_set(additional_descriptor_set)
        .push_buffer_write(0, renderer::buffer_descriptor_info_t{.handle_buffer=additional_buffer})
        .commit();
}

void renderer_t::render(editor_camera_t& camera, const std::vector<gpu_mesh_t>& gpu_meshes) {
    base_renderer.begin(false);

    auto swapchain_image_resource = rendergraph::resource_builder_t{"swapchain image"}.is_image(base_renderer.current_swapchain_image(), base_renderer.current_swapchain_image_view());
    // auto image_resource = rendergraph::resource_builder_t{"image"}.is_image(target_image, target_image_view, sampler);
    auto image_resource = rendergraph::resource_builder_t{"image"}.is_image(target_image, target_image_view); 
    auto camera_resource = rendergraph::resource_builder_t{"camera"}.is_buffer(camera_buffer);
    auto additional_resource = rendergraph::resource_builder_t{"additional"}.is_buffer(additional_buffer);

    rendergraph::rendergraph_t rendergraph{};
    rendergraph.add_task(rendergraph::task_builder_t{"camera and additional update task", rendergraph::task_type_t::e_cpu}
                    .add_resource_refrenced_and_its_usage(camera_resource, rendergraph::resource_usage_t::e_writing)
                    .add_resource_refrenced_and_its_usage(additional_resource, rendergraph::resource_usage_t::e_writing)
                    .with_callback([&](renderer::base_renderer_t& base_renderer) {
                        auto camera_uniform = reinterpret_cast<camera_uniform_t *>(base_renderer.context.map_buffer(base_renderer.buffer(camera_buffer)))
                        camera_uniform->view = camera.
                    }));

    base_renderer.end();
}