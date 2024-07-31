#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "core/core.hpp"
#include "core/window.hpp"
#include "core/ecs.hpp"
#include "core/event.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/bindless_manager.hpp"

#include "event_types.hpp"

namespace runtime {

struct transform_t {
    core::mat4 model;
    core::mat4 inv_model;
};

struct material_t {
    uint32_t albedo;     // uint32_t
};

struct camera_t {
    core::mat4 view;
    core::mat4 inv_view;
    core::mat4 projection;
    core::mat4 inv_projection;
};

struct mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;
    VkDeviceAddress vertices;
    VkDeviceAddress indices;
    uint32_t transform_index;
    uint32_t material_index;
};

struct push_constant_t {
    VkDeviceAddress vertices;
    VkDeviceAddress indices;
    uint32_t transform_index;
    uint32_t material_index;
    VkDeviceAddress transforms;
    VkDeviceAddress materials;
    VkDeviceAddress camera;
};

} // namespace runtime


class renderer_t {
public:
    renderer_t(renderer::base_renderer_t& base_renderer, gfx::handle_sampler_t sampler, uint32_t width, uint32_t height) : _base_renderer(base_renderer) {
        bindless_manager = core::make_ref<gfx::bindless_manager_t>(base_renderer.context);

        bindless_manager->slot(sampler);

        gfx::config_sampler_t c_sampler{};
        c_sampler.vk_min_filter = VK_FILTER_NEAREST;
        c_sampler.vk_mag_filter = VK_FILTER_NEAREST;
        c_sampler.vk_mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        bindless_manager->slot(_base_renderer.context.create_sampler(c_sampler));

        // image = gfx::helper::load_image_from_path(_base_renderer.context, _base_renderer.command_pool, "../../assets/texture/noise.jpg", VK_FORMAT_R8G8B8A8_SRGB);
        // image_view = _base_renderer.context.create_image_view({ .handle_image = image, .debug_name = "(../../assets/texture/noise.jpg)view"});

        std::tie(image, image_view) = gfx::helper::create_2D_image_and_image_view(_base_renderer.context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "rendered");
        std::tie(depth, depth_view) = gfx::helper::create_2D_image_and_image_view(_base_renderer.context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "depth");
        std::tie(position, position_view) = gfx::helper::create_2D_image_and_image_view(_base_renderer.context, width, height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "position");
        std::tie(normal, normal_view) = gfx::helper::create_2D_image_and_image_view(_base_renderer.context, width, height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "normal");
        std::tie(uv, uv_view) = gfx::helper::create_2D_image_and_image_view(_base_renderer.context, width, height, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "uv");
        std::tie(material, material_view) = gfx::helper::create_2D_image_and_image_view(_base_renderer.context, width, height, VK_FORMAT_R32_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "material");

        default_image = gfx::helper::load_image_from_path(_base_renderer.context, _base_renderer.command_pool, "../../assets/texture/default.png", VK_FORMAT_R8G8B8A8_SRGB);
        default_image_view = _base_renderer.context.create_image_view({ .handle_image = default_image, .debug_name = "../../assets/texture/default.png::view" });
        default_index = bindless_manager->slot(default_image_view);

        bindless_manager->slot(position_view);
        bindless_manager->slot(normal_view);
        bindless_manager->slot(uv_view);
        bindless_manager->slot(material_view);

        gfx::config_pipeline_layout_t c_primary_visibility_pl{};
        c_primary_visibility_pl.add_descriptor_set_layout(bindless_manager->dsl());
        c_primary_visibility_pl.add_push_constant(sizeof(runtime::push_constant_t), VK_SHADER_STAGE_ALL);
        primary_visibility_pl = _base_renderer.context.create_pipeline_layout(c_primary_visibility_pl);

        gfx::config_pipeline_t c_primary_visibility_p{};
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32G32B32A32_SFLOAT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32G32B32A32_SFLOAT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32G32_SFLOAT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32_UINT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.set_depth_attachment(VK_FORMAT_D32_SFLOAT, VkPipelineDepthStencilStateCreateInfo{
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
        c_primary_visibility_p.handle_pipeline_layout = primary_visibility_pl;  
        c_primary_visibility_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/primary_visibility/vertex.slang", .is_code = false, .name = "primary visibility vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
        c_primary_visibility_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/primary_visibility/fragment.slang", .is_code = false, .name = "primary visibility fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
        primary_visibility_p = _base_renderer.context.create_graphics_pipeline(c_primary_visibility_p);

        gfx::config_pipeline_layout_t c_shade_pl{};
        c_shade_pl.add_descriptor_set_layout(bindless_manager->dsl());
        c_shade_pl.add_push_constant(sizeof(runtime::push_constant_t), VK_SHADER_STAGE_ALL);
        shade_pl = _base_renderer.context.create_pipeline_layout(c_shade_pl);

        gfx::config_pipeline_t c_shade_p{};
        c_shade_p.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
        c_shade_p.handle_pipeline_layout = shade_pl;  
        c_shade_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/vertex.slang", .is_code = false, .name = "primary visibility vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
        c_shade_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/fragment.slang", .is_code = false, .name = "primary visibility fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
        shade_p = _base_renderer.context.create_graphics_pipeline(c_shade_p);

        gfx::config_buffer_t c_temp_buffer{};
        c_temp_buffer.vk_size = 1024 * 1024 * 2;
        c_temp_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        temp_buffer = _base_renderer.context.create_buffer(c_temp_buffer);
    }   

    void reload() {
        gfx::config_pipeline_layout_t c_primary_visibility_pl{};
        c_primary_visibility_pl.add_descriptor_set_layout(bindless_manager->dsl());
        c_primary_visibility_pl.add_push_constant(sizeof(runtime::push_constant_t), VK_SHADER_STAGE_ALL);
        primary_visibility_pl = _base_renderer.context.create_pipeline_layout(c_primary_visibility_pl);

        gfx::config_pipeline_t c_primary_visibility_p{};
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32G32B32A32_SFLOAT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32G32B32A32_SFLOAT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32G32_SFLOAT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.add_color_attachment(VK_FORMAT_R32_UINT, gfx::default_color_blend_attachment());
        c_primary_visibility_p.set_depth_attachment(VK_FORMAT_D32_SFLOAT, VkPipelineDepthStencilStateCreateInfo{
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
        c_primary_visibility_p.handle_pipeline_layout = primary_visibility_pl;  
        c_primary_visibility_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/primary_visibility/vertex.slang", .is_code = false, .name = "primary visibility vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
        c_primary_visibility_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/primary_visibility/fragment.slang", .is_code = false, .name = "primary visibility fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
        primary_visibility_p = _base_renderer.context.create_graphics_pipeline(c_primary_visibility_p);

        gfx::config_pipeline_layout_t c_shade_pl{};
        c_shade_pl.add_descriptor_set_layout(bindless_manager->dsl());
        c_shade_pl.add_push_constant(sizeof(runtime::push_constant_t), VK_SHADER_STAGE_ALL);
        shade_pl = _base_renderer.context.create_pipeline_layout(c_shade_pl);

        gfx::config_pipeline_t c_shade_p{};
        c_shade_p.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
        c_shade_p.handle_pipeline_layout = shade_pl;  
        c_shade_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/vertex.slang", .is_code = false, .name = "primary visibility vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
        c_shade_p.add_shader(_base_renderer.context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/application/shade/fragment.slang", .is_code = false, .name = "primary visibility fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
        shade_p = _base_renderer.context.create_graphics_pipeline(c_shade_p);
    }

    // calling this multiple times is  bad lmao
    void prepare(core::scene_t& scene) {

        r_materials.push_back({ default_index });

        scene.for_all<core::model_t, core::transform_t>([this, &scene](core::entity_id_t id, core::model_t& model, core::transform_t& transform) {
            uint32_t transform_index = r_transforms.size();
            runtime::transform_t r_transform{};
            r_transform.model = transform.mat4();
            r_transform.inv_model = core::inverse(r_transform.model);
            r_transforms.push_back(r_transform);

            // actual data to gpu

            for (auto& mesh : model.meshes) {
                gfx::handle_buffer_t vertex_buffer = gfx::helper::create_and_push_staged_vector(_base_renderer.context, _base_renderer.command_pool, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, {}, mesh.vertices);
                gfx::handle_buffer_t index_buffer = gfx::helper::create_and_push_staged_vector(_base_renderer.context, _base_renderer.command_pool, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, {}, mesh.indices);
                _base_renderer.context.flush_buffer(vertex_buffer);
                _base_renderer.context.flush_buffer(index_buffer);

                runtime::material_t r_material{};
                auto itr = std::find_if(mesh.material_description.texture_infos.begin(), mesh.material_description.texture_infos.end(), [](const core::texture_info_t& info) {
                    return info.texture_type == core::texture_type_t::e_diffuse_map;
                });
                if (itr == mesh.material_description.texture_infos.end()) {
                    r_material.albedo = default_index.val;
                } else {
                    auto image = gfx::helper::load_image_from_path(_base_renderer.context, _base_renderer.command_pool, itr->file_path, VK_FORMAT_R8G8B8A8_SRGB);
                    auto image_view = _base_renderer.context.create_image_view({ .handle_image = image });
                    r_material.albedo = bindless_manager->slot(image_view).val;
                }
                uint32_t material_index = r_materials.size();
                r_materials.push_back(r_material);

                runtime::mesh_t r_mesh{};
                r_mesh.vertices = _base_renderer.context.get_buffer_device_address(vertex_buffer);
                r_mesh.indices = _base_renderer.context.get_buffer_device_address(index_buffer);
                r_mesh.transform_index = transform_index;
                r_mesh.material_index = material_index;
                r_mesh.vertex_buffer = vertex_buffer;
                r_mesh.index_buffer = index_buffer;
                
                auto child_id = scene.create();
                scene.construct<runtime::mesh_t>(child_id) = r_mesh;
                scene.construct<core::mesh_t>(child_id) = mesh;
            }
        });      

        // upload
        transform_buffer = gfx::helper::create_and_push_staged_vector(_base_renderer.context, _base_renderer.command_pool, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, {}, r_transforms);
        material_buffer = gfx::helper::create_and_push_staged_vector(_base_renderer.context, _base_renderer.command_pool, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, {}, r_materials);

        _base_renderer.context.flush_buffer(transform_buffer);
        _base_renderer.context.flush_buffer(material_buffer);

        gfx::config_buffer_t c_camera_buffer{}; 
        c_camera_buffer.vk_size = sizeof(runtime::camera_t);
        c_camera_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        c_camera_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        camera_buffer = _base_renderer.context.create_buffer(c_camera_buffer);

        bindless_manager->flush_updates();
    }

    void render(gfx::handle_commandbuffer_t cbuff, core::scene_t& scene, core::camera_t& camera, uint32_t width, uint32_t height) {
        _base_renderer.context.cmd_copy_buffer(cbuff, transform_buffer, temp_buffer, { .vk_size = sizeof(runtime::transform_t) * r_transforms.size() });
        _base_renderer.context.cmd_copy_buffer(cbuff, material_buffer, temp_buffer, { .vk_size = sizeof(runtime::material_t) * r_materials.size() });

        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        runtime::camera_t *r_camera = reinterpret_cast<runtime::camera_t *>(_base_renderer.context.map_buffer(camera_buffer));
        r_camera->view = camera.view();
        r_camera->inv_view = camera.inv_view();
        r_camera->projection = camera.projection();             
        r_camera->inv_projection = camera.inv_projection();

        // image memory barrier 

        _base_renderer.context.cmd_image_memory_barrier(cbuff, depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, position, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, normal, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, uv, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, material, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        gfx::rendering_attachment_t attachment_0{};
        attachment_0.handle_image_view = position_view;
        gfx::rendering_attachment_t attachment_1{};
        attachment_1.handle_image_view = normal_view;
        gfx::rendering_attachment_t attachment_2{};
        attachment_2.handle_image_view = uv_view;
        gfx::rendering_attachment_t attachment_3{};
        attachment_3    .handle_image_view = material_view;
        gfx::rendering_attachment_t depth_attachment{};
        depth_attachment.handle_image_view = depth_view;
        depth_attachment.clear_value.depthStencil.depth = 1;
        depth_attachment.image_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        
        // for debug
        scene.for_all<core::mesh_t, runtime::mesh_t>([&](auto id, core::mesh_t& mesh, runtime::mesh_t& r_mesh) {
            _base_renderer.context.cmd_copy_buffer(cbuff, r_mesh.vertex_buffer, temp_buffer, { .vk_size = sizeof(core::vertex_t) * mesh.vertices.size() });
            _base_renderer.context.cmd_copy_buffer(cbuff, r_mesh.index_buffer, temp_buffer, { .vk_size = sizeof(uint32_t        ) * mesh.indices.size() });
        });

        _base_renderer.context.cmd_begin_rendering(cbuff, { attachment_0, attachment_1, attachment_2, attachment_3 }, depth_attachment, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });

        // draw
        _base_renderer.context.cmd_bind_pipeline(cbuff, primary_visibility_p);
        _base_renderer.context.cmd_set_viewport_and_scissor(cbuff, viewport, scissor);
        _base_renderer.context.cmd_bind_descriptor_sets(cbuff, primary_visibility_p, 0, { bindless_manager->ds() });
        scene.for_all<core::mesh_t, runtime::mesh_t>([&](auto id, core::mesh_t& mesh, runtime::mesh_t& r_mesh) {
            runtime::push_constant_t push_constant{};
            push_constant.camera = _base_renderer.context.get_buffer_device_address(camera_buffer);
            push_constant.materials = _base_renderer.context.get_buffer_device_address(material_buffer);
            push_constant.transforms = _base_renderer.context.get_buffer_device_address(transform_buffer);
            push_constant.vertices = r_mesh.vertices;
            push_constant.indices = r_mesh.indices;
            push_constant.transform_index = r_mesh.transform_index;
            push_constant.material_index = r_mesh.material_index;
            _base_renderer.context.cmd_push_constants(cbuff, primary_visibility_p, VK_SHADER_STAGE_ALL, 0, sizeof(runtime::push_constant_t), &push_constant);
            _base_renderer.context.cmd_draw(cbuff, mesh.indices.size(), 1, 0, 0);
        });

        _base_renderer.context.cmd_end_rendering(cbuff);

        _base_renderer.context.cmd_image_memory_barrier(cbuff, depth, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, position, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, normal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, uv, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        _base_renderer.context.cmd_image_memory_barrier(cbuff, material, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        _base_renderer.context.cmd_image_memory_barrier(cbuff, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        attachment_0 = {};
        attachment_0.handle_image_view = image_view;

        _base_renderer.context.cmd_begin_rendering(cbuff, { attachment_0 }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });

        _base_renderer.context.cmd_bind_pipeline(cbuff, shade_p);
        _base_renderer.context.cmd_set_viewport_and_scissor(cbuff, viewport, scissor);
        _base_renderer.context.cmd_bind_descriptor_sets(cbuff, shade_p, 0, { bindless_manager->ds() });
        runtime::push_constant_t push_constant{};
        push_constant.camera = _base_renderer.context.get_buffer_device_address(camera_buffer);
        push_constant.materials = _base_renderer.context.get_buffer_device_address(material_buffer);
        push_constant.transforms = _base_renderer.context.get_buffer_device_address(transform_buffer);
        push_constant.vertices = 0;
        push_constant.indices = 0;
        push_constant.transform_index = 0;
        push_constant.material_index = 0;
        _base_renderer.context.cmd_push_constants(cbuff, shade_p, VK_SHADER_STAGE_ALL, 0, sizeof(runtime::push_constant_t), &push_constant);
        _base_renderer.context.cmd_draw(cbuff, 6, 1, 0, 0);

        _base_renderer.context.cmd_end_rendering(cbuff);

        _base_renderer.context.cmd_image_memory_barrier(cbuff, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    }

    gfx::handle_image_view_t rendered_image_view() { return image_view; }

private:
    renderer::base_renderer_t& _base_renderer;
    
    core::ref<gfx::bindless_manager_t> bindless_manager;

    std::vector<runtime::transform_t> r_transforms;
    std::vector<runtime::material_t> r_materials;

    gfx::handle_buffer_t transform_buffer;
    gfx::handle_buffer_t material_buffer;
    gfx::handle_buffer_t camera_buffer;
    gfx::handle_buffer_t temp_buffer;

    gfx::handle_image_t image;
    gfx::handle_image_view_t image_view;

    gfx::handle_image_t default_image;
    gfx::handle_image_view_t default_image_view;
    gfx::handle_sampled_image_slot_t default_index;

    gfx::handle_image_t depth;
    gfx::handle_image_view_t depth_view;
    gfx::handle_image_t position;
    gfx::handle_image_view_t position_view;
    gfx::handle_image_t normal;
    gfx::handle_image_view_t normal_view;
    gfx::handle_image_t uv;
    gfx::handle_image_view_t uv_view;
    gfx::handle_image_t material;
    gfx::handle_image_view_t material_view;

    gfx::handle_pipeline_layout_t primary_visibility_pl;
    gfx::handle_pipeline_t primary_visibility_p;

    gfx::handle_pipeline_layout_t shade_pl;
    gfx::handle_pipeline_t shade_p;
};

#endif