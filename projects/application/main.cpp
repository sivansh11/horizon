#include "core/core.hpp"
#include "core/window.hpp"
#include "core/ecs.hpp"
#include "core/model.hpp"
#include "core/components.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/helper.hpp"
#include "gfx/bindless_manager.hpp"

#include "utility.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace runtime {

struct material_t {
    gfx::handle_sampled_image_slot_t albedo;  // id into bindless array
};

// this will be a uniform 
struct mesh_t {
    uint32_t material_id;
    uint32_t model_transform_id;
    VkDeviceAddress vertices;
    VkDeviceAddress indices;
};

struct model_transform_t {
    core::mat4 model;
    core::mat4 inv_model;
};

struct camera_t {
    core::mat4 view;
    core::mat4 inv_view;
    core::mat4 projection;
    core::mat4 inv_projection;
};

} // namespace runtime


int main(int argc, char **argv) {
    
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "application", 800, 800 };
    gfx::context_t context{ true };
    auto [width, height] = window.dimensions();
    auto [o_final, o_final_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto sampler = context.create_sampler({});
    renderer::base_renderer_t renderer{ window, context, sampler, o_final_view };
    gfx::bindless_manager_t bindless_manager{ context };
    gfx::helper::imgui_init(window, renderer, VK_FORMAT_R8G8B8A8_SRGB);

    core::scene_t scene{};

    {
        auto id = scene.create();
        scene.construct<core::model_t>(id) = core::load_model_from_path("../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
        scene.construct<core::transform_t>(id) = {};
    }

    // preparing gpu data
    gfx::config_sampler_t c_material_sampler{};
    c_material_sampler.vk_mag_filter = VK_FILTER_NEAREST;
    c_material_sampler.vk_min_filter = VK_FILTER_NEAREST;
    c_material_sampler.vk_mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    gfx::handle_sampler_t material_sampler = context.create_sampler(c_material_sampler);
    bindless_manager.slot(material_sampler);

    // gfx::config_descriptor_set_layout_t c_mesh_dsl{};
    // c_mesh_dsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL);
    // gfx::handle_descriptor_set_layout_t mesh_dsl = context.create_descriptor_set_layout(c_mesh_dsl);

    gfx::config_pipeline_layout_t c_test_pipeline_layout{};
    c_test_pipeline_layout.add_push_constant(sizeof(runtime::mesh_t), VK_SHADER_STAGE_ALL);
    gfx::handle_pipeline_layout_t test_pipeline_layout = context.create_pipeline_layout(c_test_pipeline_layout);

    gfx::config_pipeline_t c_test_pipeline{};
    c_test_pipeline.handle_pipeline_layout = test_pipeline_layout;
    c_test_pipeline.add_shader(context.create_shader({ .code_or_path = "../../assets/shaders/application/test.slang", .is_code = false, .name = "test", .type = gfx::shader_type_t::e_compute, .language = gfx::shader_language_t::e_slang }));
    gfx::handle_pipeline_t test_pipeline = context.create_compute_pipeline(c_test_pipeline);

    gfx::config_pipeline_layout_t c_primary_visibility_pipelne_layout{};
    c_primary_visibility_pipelne_layout.add_descriptor_set_layout(bindless_manager.dsl());
    c_primary_visibility_pipelne_layout.add_push_constant(sizeof(runtime::mesh_t), VK_SHADER_STAGE_ALL);
    gfx::handle_pipeline_layout_t primary_visibility_pipelne_layout = context.create_pipeline_layout(c_primary_visibility_pipelne_layout);

    auto default_image = gfx::helper::load_image_from_path(context, renderer.command_pool, "../../assets/texture/default.png", VK_FORMAT_R8G8B8A8_SRGB);
    auto default_image_view = context.create_image_view({ .handle_image = default_image });
    auto default_image_slot_id = bindless_manager.slot(default_image_view);

    auto [o_position, o_position_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto [o_normal, o_normal_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto [o_uv, o_uv_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto [o_material_id, o_material_id_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R32_UINT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto [o_depth, o_depth_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    
    bindless_manager.slot(o_position_view);  // 1
    bindless_manager.slot(o_normal_view);  // 2
    bindless_manager.slot(o_uv_view);  // 3 
    bindless_manager.slot(o_material_id_view);  // 4
    bindless_manager.slot(o_depth_view);  // 5

    gfx::config_pipeline_t c_primary_visibility_pipeline{};
    c_primary_visibility_pipeline.handle_pipeline_layout = primary_visibility_pipelne_layout;
    c_primary_visibility_pipeline.add_color_attachment(VK_FORMAT_R32G32B32A32_SFLOAT, gfx::default_color_blend_attachment());
    c_primary_visibility_pipeline.add_color_attachment(VK_FORMAT_R32G32B32A32_SFLOAT, gfx::default_color_blend_attachment());
    c_primary_visibility_pipeline.add_color_attachment(VK_FORMAT_R32G32_SFLOAT, gfx::default_color_blend_attachment());
    c_primary_visibility_pipeline.add_color_attachment(VK_FORMAT_R32_UINT, gfx::default_color_blend_attachment());
    c_primary_visibility_pipeline.set_depth_attachment(VK_FORMAT_D32_SFLOAT, VkPipelineDepthStencilStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = {},
        .flags = {},
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_TRUE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0,
        .maxDepthBounds = 1,
    });
    c_primary_visibility_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/primary_visibility/vert.slang", .is_code = false, .name = "primary visibility vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
    c_primary_visibility_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/primary_visibility/frag.slang", .is_code = false, .name = "primary visibility fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
    gfx::handle_pipeline_t primary_visibility_pipeline = context.create_graphics_pipeline(c_primary_visibility_pipeline);

    gfx::config_pipeline_t c_shade_pipeline{};
    c_shade_pipeline.handle_pipeline_layout = bindless_manager.pl();
    c_shade_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
    c_shade_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/vert.slang", .is_code = false, .name = "shade vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
    c_shade_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/frag.slang", .is_code = false, .name = "shade fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
    gfx::handle_pipeline_t shade_pipeline = context.create_graphics_pipeline(c_shade_pipeline);
    
    std::map<core::entity_id_t, uint32_t> entity_to_transform_map;
    std::vector<runtime::model_transform_t> model_transforms;
    std::vector<runtime::material_t> materials;
    materials.emplace({});  // 0 is null
    scene.for_all<core::model_t, core::transform_t>([&](auto id, core::model_t& model, core::transform_t& transform) {
        
        uint32_t model_transform_id = model_transforms.size();
        entity_to_transform_map[id] = model_transform_id;

        runtime::model_transform_t model_transform{};
        model_transform.model = transform.mat4();
        model_transform.inv_model = core::inverse(model_transform.model);
        model_transforms.emplace_back(model_transform);

        for (auto& mesh : model.meshes) {
            auto mesh_ent_id = scene.create();
            runtime::mesh_t& r_mesh = scene.construct<runtime::mesh_t>(mesh_ent_id);

            scene.construct<core::mesh_t>(mesh_ent_id) = mesh;

            gfx::config_buffer_t config_vertex_buffer{};
            config_vertex_buffer.vk_size = mesh.vertices.size() * sizeof(core::vertex_t);
            config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            config_vertex_buffer.vma_allocation_create_flags = {};
            gfx::handle_buffer_t vertex_buffer = gfx::helper::create_and_push_buffer_staged(context, renderer.command_pool, config_vertex_buffer, mesh.vertices.data());

            gfx::config_buffer_t config_index_buffer{};
            config_index_buffer.vk_size = mesh.indices.size() * sizeof(uint32_t);
            config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            config_index_buffer.vma_allocation_create_flags = {};
            gfx::handle_buffer_t index_buffer = gfx::helper::create_and_push_buffer_staged(context, renderer.command_pool, config_index_buffer, mesh.indices.data());
            
            runtime::material_t r_material{};

            auto itr = std::find_if(mesh.material_description.texture_infos.begin(), mesh.material_description.texture_infos.end(),
                [](const core::texture_info_t& texture_info) {
                    return texture_info.texture_type == core::texture_type_t::e_diffuse_map;
                });

            if (itr != mesh.material_description.texture_infos.end()) {
                auto albedo = gfx::helper::load_image_from_path(context, renderer.command_pool, itr->file_path, VK_FORMAT_R8G8B8A8_SRGB);
                auto albedo_view = context.create_image_view({ .handle_image = albedo });
                r_material.albedo = bindless_manager.slot(albedo_view);
            } else {
                r_material.albedo = default_image_slot_id;
            }

            uint32_t model_transform_id = model_transforms.size();
            entity_to_transform_map[mesh_ent_id] = model_transform_id;

            runtime::model_transform_t model_transform{};
            model_transform.model = transform.mat4();
            model_transform.inv_model = core::inverse(model_transform.model);
            model_transforms.emplace_back(model_transform);

            r_mesh.vertices = context.get_buffer_device_address(vertex_buffer);
            r_mesh.indices = context.get_buffer_device_address(index_buffer);
            r_mesh.model_transform_id = model_transform_id;
            r_mesh.material_id = materials.size();

            materials.push_back(r_material);

            // gfx::config_buffer_t c_uniform_buffer{};
            // c_uniform_buffer.vk_size = sizeof(runtime::mesh_t);
            // c_uniform_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            // c_uniform_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            // gfx::handle_buffer_t uniform_buffer = gfx::helper::create_and_push_buffer_staged(context, renderer.command_pool, c_uniform_buffer, &r_mesh);

            // context.flush_buffer(uniform_buffer);

            // gfx::handle_descriptor_set_t mesh_ds = context.allocate_descriptor_set({ .handle_descriptor_set_layout = mesh_dsl });
            // context.update_descriptor_set(mesh_ds)
            //     .push_buffer_write(0, { .handle_buffer = uniform_buffer })
            //     .commit();

            // scene.construct<gfx::handle_descriptor_set_t>(mesh_ent_id) = mesh_ds;
            // scene.construct<gfx::handle_buffer_t>(mesh_ent_id) = uniform_buffer;
        }
    });

    // scene.for_all<gfx::handle_buffer_t>([&](auto id, gfx::handle_buffer_t handle) {
    //     runtime::mesh_t *mesh = reinterpret_cast<runtime::mesh_t *>(context.map_buffer(handle));
    //     horizon_info("{}", mesh->material_id);
    // });

    gfx::handle_buffer_t model_transform_buffer = gfx::helper::create_and_push_staged_vector(context, renderer.command_pool, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {}, model_transforms);
    gfx::handle_buffer_t material_buffer = gfx::helper::create_and_push_staged_vector(context, renderer.command_pool, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {}, materials);

    gfx::config_buffer_t c_camera_buffer{};
    c_camera_buffer.vk_size = sizeof(runtime::camera_t);
    c_camera_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    c_camera_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    gfx::handle_buffer_t camera_buffer = context.create_buffer(c_camera_buffer);

    bindless_manager.slot(model_transform_buffer, 999);
    bindless_manager.slot(material_buffer, 998);
    bindless_manager.slot(camera_buffer, 997);

    bindless_manager.flush_updates();

    context.wait_idle();

    float target_fps = 10000000.f;
    auto last_time = std::chrono::system_clock::now();
    core::frame_timer_t frame_timer{ 60.f };
    core::camera_t camera{};
    camera_controller_t controller{ window };


    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) break;
        if (glfwGetKey(window, GLFW_KEY_R)) {
            context.wait_idle();
            context.destroy_pipeline(shade_pipeline);
            gfx::config_pipeline_t c_shade_pipeline{};
            c_shade_pipeline.handle_pipeline_layout = bindless_manager.pl();
            c_shade_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
            c_shade_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/vert.slang", .is_code = false, .name = "shade vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
            c_shade_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/frag.slang", .is_code = false, .name = "shade fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
            shade_pipeline = context.create_graphics_pipeline(c_shade_pipeline);
        }

        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_fps) {
            continue;
        }
        last_time = current_time;

        auto dt = frame_timer.update();
        controller.update(dt.count(), camera);

        // this should be double buffed, but fuck it
        runtime::camera_t *r_camera = reinterpret_cast<runtime::camera_t *>(context.map_buffer(camera_buffer));
        r_camera->view = camera.view();
        r_camera->inv_view = camera.inv_view();
        r_camera->projection = camera.projection();
        r_camera->inv_projection = camera.inv_projection();

        renderer.begin();
        auto cbuff = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        context.cmd_image_memory_barrier(cbuff, o_position, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_image_memory_barrier(cbuff, o_normal, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_image_memory_barrier(cbuff, o_uv, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_image_memory_barrier(cbuff, o_material_id, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_image_memory_barrier(cbuff, o_depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

        // mega barrier 
        VkMemoryBarrier vk_memory_barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        vk_memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        vk_memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        context.cmd_pipeline_barrier(cbuff, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, { vk_memory_barrier }, {}, {});

        gfx::rendering_attachment_t position_attachment{};
        position_attachment.clear_value = {0, 0, 0, 0};
        position_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        position_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        position_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        position_attachment.handle_image_view = o_position_view;
        gfx::rendering_attachment_t normal_attachment{};
        normal_attachment.clear_value = {0, 0, 0, 0};
        normal_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        normal_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        normal_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        normal_attachment.handle_image_view = o_normal_view;
        gfx::rendering_attachment_t uv_attachment{};
        uv_attachment.clear_value = {0, 0, 0, 0};
        uv_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        uv_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        uv_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        uv_attachment.handle_image_view = o_uv_view;
        gfx::rendering_attachment_t material_id_attachment{};
        material_id_attachment.clear_value.color.uint32[0] = 0;
        material_id_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        material_id_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        material_id_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        material_id_attachment.handle_image_view = o_material_id_view;
        gfx::rendering_attachment_t depth_attachment{};
        depth_attachment.clear_value.depthStencil = { 1 };
        depth_attachment.image_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.handle_image_view = o_depth_view;
        context.cmd_begin_rendering(cbuff, { position_attachment, normal_attachment, uv_attachment, material_id_attachment }, depth_attachment, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });

        // draw things
        context.cmd_bind_pipeline(cbuff, primary_visibility_pipeline);
        context.cmd_set_viewport_and_scissor(cbuff, viewport, scissor);
        context.cmd_bind_descriptor_sets(cbuff, primary_visibility_pipeline, 0, { bindless_manager.ds() });
        scene.for_all<core::mesh_t, runtime::mesh_t>([&](auto id, core::mesh_t& mesh, runtime::mesh_t& r_mesh) {
            context.cmd_push_constants(cbuff, primary_visibility_pipeline, VK_SHADER_STAGE_ALL, 0, sizeof(runtime::mesh_t), &r_mesh);
            context.cmd_draw(cbuff, mesh.indices.size(), 1, 0, 0);
        });
        
        context.cmd_end_rendering(cbuff);
     
        context.cmd_image_memory_barrier(cbuff, o_position, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        context.cmd_image_memory_barrier(cbuff, o_normal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        context.cmd_image_memory_barrier(cbuff, o_uv, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        context.cmd_image_memory_barrier(cbuff, o_material_id, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        context.cmd_image_memory_barrier(cbuff, o_depth, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        
        context.cmd_bind_pipeline(cbuff, test_pipeline);
        scene.for_all<core::mesh_t, runtime::mesh_t>([&](auto id, core::mesh_t& mesh, runtime::mesh_t& r_mesh) {
            context.cmd_push_constants(cbuff, test_pipeline, VK_SHADER_STAGE_ALL, 0, sizeof(runtime::mesh_t), &r_mesh);
            context.cmd_dispatch(cbuff, 1, 1, 1);   
        });

        context.cmd_image_memory_barrier(cbuff, o_final, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        gfx::rendering_attachment_t final_attachment{};
        final_attachment.clear_value = {0, 0, 0, 0};
        final_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        final_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        final_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        final_attachment.handle_image_view = o_final_view;

        context.cmd_begin_rendering(cbuff, { final_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });

        context.cmd_bind_pipeline(cbuff, shade_pipeline);
        context.cmd_set_viewport_and_scissor(cbuff, viewport, scissor);
        context.cmd_bind_descriptor_sets(cbuff, shade_pipeline, 0, { bindless_manager.ds() });
        context.cmd_draw(cbuff, 6, 1, 0, 0);

        gfx::helper::imgui_newframe();

        ImGui::Begin("Test");
        ImGui::End();

        gfx::helper::imgui_endframe(renderer, cbuff);

        context.cmd_end_rendering(cbuff);

        context.cmd_image_memory_barrier(cbuff, o_final, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    context.wait_idle();

    gfx::helper::imgui_shutdown();

    return 0;
}
