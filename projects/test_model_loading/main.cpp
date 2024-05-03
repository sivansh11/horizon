#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "rendergraph.hpp"
#include "utility.hpp"

struct gpu_mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;
    // material ?
};

struct renderer_t {
    renderer_t(core::window_t& window) : window(window), base_renderer( window ) {
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
        
        gfx::config_pipeline_layout_t config_test_pipeline_layout{};
        auto test_pipeline_layout = base_renderer.context.create_pipeline_layout(config_test_pipeline_layout);
        
        gfx::config_pipeline_t config_test_pipeline{};
        config_test_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/test/glsl.vert").data(), .name = "vertex_shader", .type = gfx::shader_type_t::e_vertex }));
        config_test_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/test/glsl.frag").data(), .name = "fragment_shader", .type = gfx::shader_type_t::e_fragment }));
        config_test_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
        config_test_pipeline.handle_pipeline_layout = test_pipeline_layout;
        test_pipeline = base_renderer.context.create_graphics_pipeline(config_test_pipeline);

        gfx::config_pipeline_t config_test_model_pipeline{};
        config_test_model_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/test_model/glsl.vert").data(), .name = "vertex_shader", .type = gfx::shader_type_t::e_vertex }));
        config_test_model_pipeline.add_shader(base_renderer.context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/test_model/glsl.frag").data(), .name = "fragment_shader", .type = gfx::shader_type_t::e_fragment }));
        config_test_model_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
        config_test_model_pipeline.add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
        config_test_model_pipeline.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position));
        config_test_model_pipeline.add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, normal));
        config_test_model_pipeline.add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv));
        config_test_model_pipeline.add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, tangent));
        config_test_model_pipeline.add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, bi_tangent));
        config_test_model_pipeline.handle_pipeline_layout = test_pipeline_layout;
        test_model_pipeline = base_renderer.context.create_graphics_pipeline(config_test_model_pipeline);
        
    }

    void render(const std::vector<gpu_mesh_t>& gpu_meshes) {
        base_renderer.begin(false);

        auto swapchain_image_resource = rendergraph::resource_builder_t{"swapchain image"}.is_image(base_renderer.current_swapchain_image(), base_renderer.current_swapchain_image_view());
        // auto image_resource = rendergraph::resource_builder_t{"image"}.is_image(target_image, target_image_view, sampler);
        auto image_resource = rendergraph::resource_builder_t{"image"}.is_image(target_image, target_image_view);

        rendergraph::rendergraph_t rendergraph{};
        rendergraph.add_task(rendergraph::task_builder_t{"image task", rendergraph::task_type_t::e_graphics}
                        .add_resource_refrenced_and_its_usage(image_resource, rendergraph::resource_usage_t::e_writing)
                        .with_callback([&](renderer::base_renderer_t& base_renderer) {
                            auto commandbuffer = base_renderer.current_commandbuffer();
                            gfx::rendering_attachment_t rendering_attachment{};
                            rendering_attachment.clear_value = {0, 0, 0, 0};
                            rendering_attachment.handle_image_view = target_image_view;
                            rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                            rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
                            rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
                            auto [width, height] = base_renderer.window.dimensions();
                            auto [viewport, scissor] = fill_viewport_and_scissor_structs(width, height);

                            base_renderer.context.cmd_begin_rendering(commandbuffer, {rendering_attachment}, std::nullopt, VkRect2D{VkOffset2D{}, {uint32_t(width), uint32_t(height)}});
                            // base_renderer.context.cmd_bind_pipeline(commandbuffer, test_pipeline);
                            // base_renderer.context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
                            // base_renderer.context.cmd_draw(commandbuffer, 3, 1, 0, 0);
                            base_renderer.context.cmd_bind_pipeline(commandbuffer, test_model_pipeline);
                            base_renderer.context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
                            for (auto& gpu_mesh : gpu_meshes) {
                                base_renderer.context.cmd_bind_vertex_buffers(commandbuffer, 0, { gpu_mesh.vertex_buffer }, {0});
                                base_renderer.context.cmd_bind_index_buffer(commandbuffer, gpu_mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);
                                base_renderer.context.cmd_draw_indexed(commandbuffer, gpu_mesh.index_count, 1, 0, 0, 0);
                            }
                            // base_renderer.context.cmd_draw(commandbuffer, 3, 1, 0, 0);
                            base_renderer.context.cmd_end_rendering(commandbuffer);
                        }))
                    .add_task(rendergraph::task_builder_t{"swapchain task", rendergraph::task_type_t::e_graphics}
                        .add_resource_refrenced_and_its_usage(swapchain_image_resource, rendergraph::resource_usage_t::e_writing)
                        .add_resource_refrenced_and_its_usage(image_resource, rendergraph::resource_usage_t::e_reading)
                        .with_callback([&](renderer::base_renderer_t& base_renderer) {
                            auto commandbuffer = base_renderer.current_commandbuffer();
                            auto swapchain_rendering_attachment = base_renderer.swapchain_rendering_attachment({0, 0, 0, 0}, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
                            auto [width, height] = base_renderer.window.dimensions();
                            base_renderer.context.cmd_begin_rendering(commandbuffer, {swapchain_rendering_attachment}, std::nullopt, VkRect2D{VkOffset2D{}, {640, 420}});
                            base_renderer.context.cmd_bind_pipeline(commandbuffer, swapchain_pipeline);
                            base_renderer.context.cmd_bind_descriptor_sets(commandbuffer, swapchain_pipeline, 0, { base_renderer.descriptor_set(swapchain_descriptor_set) });
                            auto [viewport, scissor] = fill_viewport_and_scissor_structs(width, height);
                            base_renderer.context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
                            base_renderer.context.cmd_draw(commandbuffer, 6, 1, 0, 0);
                            base_renderer.context.cmd_end_rendering(commandbuffer);
                        })).run(base_renderer);
        base_renderer.end();
    }

    core::window_t& window;
    renderer::base_renderer_t base_renderer;

    gfx::handle_descriptor_set_layout_t single_combined_image_sampler_descriptor_set_layout;

    gfx::handle_pipeline_layout_t swapchain_pipeline_layout;
    
    gfx::handle_pipeline_t swapchain_pipeline;
    gfx::handle_pipeline_t test_pipeline;
    gfx::handle_pipeline_t test_model_pipeline;
    
    renderer::handle_descriptor_set_t swapchain_descriptor_set;

    gfx::handle_image_t target_image;

    gfx::handle_image_view_t target_image_view;

    gfx::handle_sampler_t sampler;
};

int main() {
    core::window_t window{ "test", 640, 420 };
    auto [width, height] = window.dimensions();
    
    renderer_t renderer{ window };
    auto& context = renderer.base_renderer.context;

    auto fence = context.create_fence({});

    std::vector<gpu_mesh_t> gpu_meshes;
    auto model = core::load_model_from_path("../../assets/models/triangle/triangle.obj");
    {
        for (auto mesh : model.meshes) {
            gpu_mesh_t gpu_mesh{};

            gpu_mesh.vertex_count = mesh.vertices.size();
            gpu_mesh.index_count = mesh.indices.size();

            gfx::config_buffer_t config_vertex_buffer{};
            config_vertex_buffer.vk_size = mesh.vertices.size() * sizeof(core::vertex_t);
            config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            config_vertex_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            auto staging_vertex_buffer = context.create_buffer(config_vertex_buffer);
            config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            config_vertex_buffer.vma_allocation_create_flags = {};
            gpu_mesh.vertex_buffer = context.create_buffer(config_vertex_buffer);
            std::memcpy(context.map_buffer(staging_vertex_buffer), mesh.vertices.data(), config_vertex_buffer.vk_size);

            gfx::config_buffer_t config_index_buffer{};
            config_index_buffer.vk_size = mesh.indices.size() * sizeof(uint32_t);
            config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            config_index_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            auto staging_index_buffer = context.create_buffer(config_index_buffer);
            config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            config_index_buffer.vma_allocation_create_flags = {};
            gpu_mesh.index_buffer = context.create_buffer(config_index_buffer);
            std::memcpy(context.map_buffer(staging_index_buffer), mesh.indices.data(), config_index_buffer.vk_size);

            auto commandbuffer = context.allocate_commandbuffer({.handle_command_pool = renderer.base_renderer.command_pool});
            context.begin_commandbuffer(commandbuffer, true);
            context.cmd_copy_buffer(commandbuffer, staging_vertex_buffer, gpu_mesh.vertex_buffer, {.vk_size = config_vertex_buffer.vk_size});
            context.cmd_copy_buffer(commandbuffer, staging_index_buffer, gpu_mesh.index_buffer, {.vk_size = config_index_buffer.vk_size});
            context.end_commandbuffer(commandbuffer);

            context.reset_fence(fence);
            context.submit_commandbuffer(commandbuffer, {}, {}, {}, fence);
            context.wait_fence(fence);

            context.destroy_buffer(staging_vertex_buffer);
            context.destroy_buffer(staging_index_buffer);

            gpu_meshes.push_back(gpu_mesh);
        }   
    }

    // for (auto [name, count_and_time] : core::get_frame_function_times()) {
    //     auto [count, time] = count_and_time;
    //     horizon_info("{} took {} and was called {} times", name, time, count);
    // }

    core::frame_timer_t frame_timer{60.f};

    while (!window.should_close()) {
        core::window_t::poll_events();  
        auto dt = frame_timer.update();
        std::cout << dt.count() << ' ' << 1000.f / frame_timer._target_fps << '\n';
        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
        
        renderer.render(gpu_meshes);

    }

    return 0;
}