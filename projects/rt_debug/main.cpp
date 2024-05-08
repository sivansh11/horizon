#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"
#include "display.hpp"
#include "bvh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct gpu_mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;

    size_t vertex_count;
    size_t index_count;
};

struct model_ubo_t {
    glm::mat4 model;
    glm::mat4 inv_model;
};

struct camera_ubo_t {
    glm::mat4 projection;
    glm::mat4 inv_projection;
    glm::mat4 view;
    glm::mat4 inv_view;
};

struct object_t {
    gfx::handle_buffer_t model_buffer;
    gfx::handle_buffer_t color_buffer;
    gfx::handle_descriptor_set_t object_descriptor_set;
};

object_t new_object(gfx::context_t& context, gfx::handle_descriptor_set_layout_t descriptor_set_layout, core::rng_t& rng, const transform_t& transform) {
    object_t object{};

    gfx::config_buffer_t config_model_buffer{};
    config_model_buffer.vk_size = sizeof(model_ubo_t);
    config_model_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_model_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    object.model_buffer = context.create_buffer(config_model_buffer);

    gfx::config_buffer_t config_color_buffer{};
    config_color_buffer.vk_size = sizeof(glm::vec3);
    config_color_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_color_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    object.color_buffer = context.create_buffer(config_color_buffer);

    model_ubo_t model_ubo{};
    model_ubo.model = transform.mat4();
    model_ubo.inv_model = glm::inverse(model_ubo.model);
    std::memcpy(context.map_buffer(object.model_buffer), &model_ubo, sizeof(model_ubo_t));

    glm::vec3 color = rng.random3f() / 100.f;
    std::memcpy(context.map_buffer(object.color_buffer), &color, sizeof(glm::vec3));

    object.object_descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = descriptor_set_layout });
    context.update_descriptor_set(object.object_descriptor_set)
        .push_buffer_write(0, { .handle_buffer = object.model_buffer })
        .push_buffer_write(1, { .handle_buffer = object.color_buffer })
        .commit();

    return object;
}

int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "rt debug", 800, 800 };
    gfx::context_t context{ true };

    auto [width, height] = window.dimensions();

    gfx::config_image_t config_target_image{};
    config_target_image.vk_width = width;
    config_target_image.vk_height = height;
    config_target_image.vk_depth = 1;
    config_target_image.vk_type = VK_IMAGE_TYPE_2D;
    config_target_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    config_target_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_target_image.vk_mips = 1;
    gfx::handle_image_t target_image = context.create_image(config_target_image);
    gfx::handle_image_view_t target_image_view = context.create_image_view({ .handle_image = target_image });

    gfx::handle_sampler_t sampler = context.create_sampler({});

    renderer::base_renderer_t renderer{ window, context, sampler, target_image_view };

    gfx::config_descriptor_set_layout_t config_camera_descriptor_set_layout{};
    config_camera_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
    gfx::handle_descriptor_set_layout_t camera_descriptor_set_layout = context.create_descriptor_set_layout(config_camera_descriptor_set_layout); 
    
    gfx::config_descriptor_set_layout_t config_debug_raytracing_inputs_descriptor_set_layout{};
    config_debug_raytracing_inputs_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                                        .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                                        .add_layout_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t debug_raytracing_inputs_descriptor_set_layout = context.create_descriptor_set_layout(config_debug_raytracing_inputs_descriptor_set_layout); 

    gfx::config_descriptor_set_layout_t config_extra_descriptor_set_layout{};
    config_extra_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t extra_descriptor_set_layout = context.create_descriptor_set_layout(config_extra_descriptor_set_layout); 

    gfx::config_pipeline_layout_t config_debug_raytracing_pipeline_layout{};
    config_debug_raytracing_pipeline_layout.add_descriptor_set_layout(debug_raytracing_inputs_descriptor_set_layout)
                                           .add_descriptor_set_layout(camera_descriptor_set_layout)
                                           .add_descriptor_set_layout(extra_descriptor_set_layout);
    gfx::handle_pipeline_layout_t debug_raytracing_pipeline_layout = context.create_pipeline_layout(config_debug_raytracing_pipeline_layout);

    gfx::config_pipeline_t config_debug_raytracing_pipeline{};
    config_debug_raytracing_pipeline.handle_pipeline_layout = debug_raytracing_pipeline_layout;
    config_debug_raytracing_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment())
                                    .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/rt_debug/raytracing/glsl.vert").data(), .name = "debug raytracing vertex", .type = gfx::shader_type_t::e_vertex }))
                                    .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/rt_debug/raytracing/glsl.frag").data(), .name = "debug raytracing fragment", .type = gfx::shader_type_t::e_fragment }));
    gfx::handle_pipeline_t debug_raytracing_pipeline = context.create_graphics_pipeline(config_debug_raytracing_pipeline);

    gfx::config_pipeline_layout_t config_debug_triangle_pipeline_layout{};
    config_debug_triangle_pipeline_layout.add_descriptor_set_layout(camera_descriptor_set_layout);
    gfx::handle_pipeline_layout_t debug_triangle_pipeline_layout = context.create_pipeline_layout(config_debug_triangle_pipeline_layout);

    gfx::config_pipeline_t config_debug_triangle_pipeline{};
    config_debug_triangle_pipeline.handle_pipeline_layout = debug_triangle_pipeline_layout;
    config_debug_triangle_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, VkPipelineColorBlendAttachmentState{
                                                                            // VkBool32                 blendEnable;
                                                                            // VkBlendFactor            srcColorBlendFactor;
                                                                            // VkBlendFactor            dstColorBlendFactor;
                                                                            // VkBlendOp                colorBlendOp;
                                                                            // VkBlendFactor            srcAlphaBlendFactor;
                                                                            // VkBlendFactor            dstAlphaBlendFactor;
                                                                            // VkBlendOp                alphaBlendOp;
                                                                            // VkColorComponentFlags    colorWriteMask;
                                                                            .blendEnable = VK_TRUE,
                                                                            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
                                                                            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
                                                                            .colorBlendOp = VK_BLEND_OP_ADD, // Optional
                                                                            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, // Optional
                                                                            .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA, // Optional
                                                                            .alphaBlendOp = VK_BLEND_OP_ADD, // Optional)
                                                                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                                                        })
                              .add_vertex_input_binding_description(0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX)
                              .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
                            //   .add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, normal))
                            //   .add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv))
                            //   .add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, tangent))
                            //   .add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, bi_tangent))
                              .set_pipeline_rasterization_state(VkPipelineRasterizationStateCreateInfo{
                                    // VkStructureType                            sType;
                                    // const void*                                pNext;
                                    // VkPipelineRasterizationStateCreateFlags    flags;
                                    // VkBool32                                   depthClampEnable;
                                    // VkBool32                                   rasterizerDiscardEnable;
                                    // VkPolygonMode                              polygonMode;
                                    // VkCullModeFlags                            cullMode;
                                    // VkFrontFace                                frontFace;
                                    // VkBool32                                   depthBiasEnable;
                                    // float                                      depthBiasConstantFactor;
                                    // float                                      depthBiasClamp;
                                    // float                                      depthBiasSlopeFactor;
                                    // float                                      lineWidth;
                                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                    .depthClampEnable = VK_FALSE,
                                    .rasterizerDiscardEnable = VK_FALSE,
                                    .polygonMode = VK_POLYGON_MODE_LINE,
                                    .cullMode = VK_CULL_MODE_NONE,
                                    .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                    .depthBiasEnable = VK_FALSE,
                                    .depthBiasConstantFactor = 0.0f, // Optional
                                    .depthBiasClamp = 0.0f, // Optional
                                    .depthBiasSlopeFactor = 0.0f, // Optional
                                    .lineWidth = 1.0f,
                              })
                              .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/rt_debug/triangle/glsl.vert").data(), .name = "debug vertex", .type = gfx::shader_type_t::e_vertex }))
                              .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/rt_debug/triangle/glsl.frag").data(), .name = "debug fragment", .type = gfx::shader_type_t::e_fragment }));
    gfx::handle_pipeline_t debug_triangle_pipeline = context.create_graphics_pipeline(config_debug_triangle_pipeline);

    gfx::config_descriptor_set_layout_t config_object_descriptor_set_layout{};
    config_object_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    config_object_descriptor_set_layout.add_layout_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t object_descriptor_set_layout = context.create_descriptor_set_layout(config_object_descriptor_set_layout); 

    gfx::config_pipeline_layout_t config_debug_node_pipeline_layout{};
    config_debug_node_pipeline_layout.add_descriptor_set_layout(camera_descriptor_set_layout);
    config_debug_node_pipeline_layout.add_descriptor_set_layout(object_descriptor_set_layout);
    gfx::handle_pipeline_layout_t debug_node_pipeline_layout = context.create_pipeline_layout(config_debug_node_pipeline_layout);

    gfx::config_pipeline_t config_debug_node_pipeline{};
    config_debug_node_pipeline.handle_pipeline_layout = debug_node_pipeline_layout;
    config_debug_node_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, VkPipelineColorBlendAttachmentState{
                                                                            // VkBool32                 blendEnable;
                                                                            // VkBlendFactor            srcColorBlendFactor;
                                                                            // VkBlendFactor            dstColorBlendFactor;
                                                                            // VkBlendOp                colorBlendOp;
                                                                            // VkBlendFactor            srcAlphaBlendFactor;
                                                                            // VkBlendFactor            dstAlphaBlendFactor;
                                                                            // VkBlendOp                alphaBlendOp;
                                                                            // VkColorComponentFlags    colorWriteMask;
                                                                            .blendEnable = VK_TRUE,
                                                                            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
                                                                            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
                                                                            .colorBlendOp = VK_BLEND_OP_ADD, // Optional
                                                                            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, // Optional
                                                                            .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA, // Optional
                                                                            .alphaBlendOp = VK_BLEND_OP_ADD, // Optional)
                                                                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                                                        })
                              .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
                              .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
                              .add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, normal))
                              .add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv))
                              .add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, tangent))
                              .add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, bi_tangent))
                              .set_pipeline_rasterization_state(VkPipelineRasterizationStateCreateInfo{
                                    // VkStructureType                            sType;
                                    // const void*                                pNext;
                                    // VkPipelineRasterizationStateCreateFlags    flags;
                                    // VkBool32                                   depthClampEnable;
                                    // VkBool32                                   rasterizerDiscardEnable;
                                    // VkPolygonMode                              polygonMode;
                                    // VkCullModeFlags                            cullMode;
                                    // VkFrontFace                                frontFace;
                                    // VkBool32                                   depthBiasEnable;
                                    // float                                      depthBiasConstantFactor;
                                    // float                                      depthBiasClamp;
                                    // float                                      depthBiasSlopeFactor;
                                    // float                                      lineWidth;
                                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                    .depthClampEnable = VK_FALSE,
                                    .rasterizerDiscardEnable = VK_FALSE,
                                    .polygonMode = VK_POLYGON_MODE_FILL,
                                    .cullMode = VK_CULL_MODE_NONE,
                                    .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                    .depthBiasEnable = VK_FALSE,
                                    .depthBiasConstantFactor = 0.0f, // Optional
                                    .depthBiasClamp = 0.0f, // Optional
                                    .depthBiasSlopeFactor = 0.0f, // Optional
                                    .lineWidth = 1.0f,
                              })
                              .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/rt_debug/node/glsl.vert").data(), .name = "debug vertex", .type = gfx::shader_type_t::e_vertex }))
                              .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/rt_debug/node/glsl.frag").data(), .name = "debug fragment", .type = gfx::shader_type_t::e_fragment }));
    gfx::handle_pipeline_t debug_node_pipeline = context.create_graphics_pipeline(config_debug_node_pipeline);

    gfx::config_buffer_t config_camera_buffer{};
    config_camera_buffer.vk_size = sizeof(camera_ubo_t);
    config_camera_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_camera_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    renderer::handle_buffer_t camera_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_camera_buffer);

    renderer::handle_descriptor_set_t camera_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = camera_descriptor_set_layout });
    renderer.update_descriptor_set(camera_descriptor_set)
        .push_buffer_write(0, { .handle_buffer = camera_buffer })
        .commit();

    auto model = core::load_model_from_path("../../assets/models/cube.glb");
    std::vector<gpu_mesh_t> cube_meshes;
    {
        for (auto& mesh : model.meshes) {
            gpu_mesh_t gpu_mesh{};

            gfx::config_buffer_t config_vertex_buffer{};
            config_vertex_buffer.vk_size = sizeof(core::vertex_t) * mesh.vertices.size();
            config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            gpu_mesh.vertex_buffer = gfx::helper::create_and_push_buffer_staged(context, renderer.command_pool, config_vertex_buffer, mesh.vertices.data());

            gfx::config_buffer_t config_index_buffer{};
            config_index_buffer.vk_size = sizeof(uint32_t) * mesh.indices.size();
            config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            gpu_mesh.index_buffer = gfx::helper::create_and_push_buffer_staged(context, renderer.command_pool, config_index_buffer, mesh.indices.data());

            gpu_mesh.vertex_count = mesh.vertices.size();
            gpu_mesh.index_count = mesh.indices.size();

            cube_meshes.push_back(gpu_mesh);
        }
    }

    auto rt_model = core::load_model_from_path("../../assets/models/cornell_box.obj");
    std::vector<triangle_t> triangles;
    {
        for (auto& mesh : rt_model.meshes) {
            check(mesh.indices.size() % 3 == 0, "indices count is not a multiple of 3");
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                core::vertex_t v0 = mesh.vertices[mesh.indices[i + 0]];
                core::vertex_t v1 = mesh.vertices[mesh.indices[i + 1]];
                core::vertex_t v2 = mesh.vertices[mesh.indices[i + 2]];

                triangle_t triangle;
                triangle.p0 = v0.position;
                triangle.p1 = v1.position;
                triangle.p2 = v2.position;

                triangles.push_back(triangle);
            }
        }
    }
    
    // horizon_info("Loaded file with {} triangle(s)", triangles.size());
    // horizon_info("Triangles");
    // for (auto& triangle : triangles) {
    //     horizon_info("p0: {} p1: {} p2: {}", glm::to_string(triangle.p0), glm::to_string(triangle.p1), glm::to_string(triangle.p2));
    // }

    std::vector<bounding_box_t> aabbs(triangles.size());
    std::vector<glm::vec3> centers(triangles.size());
    
    for (size_t i = 0; i < triangles.size(); i++) {
        aabbs[i] = bounding_box_t{}.extend(triangles[i].p0).extend(triangles[i].p1).extend(triangles[i].p2);
        centers[i] = (triangles[i].p0 + triangles[i].p1 + triangles[i].p2) / 3.0f;
    }

    bvh_t bvh = bvh_t::build(aabbs.data(), centers.data(), triangles.size());

    horizon_info("Built BVH with {} node(s) and depth {}", bvh.nodes.size(), bvh.depth());
    // horizon_info("Nodes");
    // for (auto& node : bvh.nodes) {
    //     horizon_info("bb min: {}, max: {}, primitive_count: {}, first_index: {}", glm::to_string(node.bounding_box.min), glm::to_string(node.bounding_box.max), node.primitive_count, node.first_index);
    // }

    horizon_info("Primitive Indices");
    // for (auto& id : bvh.primitive_indices) {
    //     horizon_info("{}", id);
    // }

    gfx::config_buffer_t config_triangle_buffer{};
    config_triangle_buffer.vk_size = sizeof(triangle_t) * triangles.size();
    config_triangle_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_triangle_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t triangle_buffer = context.create_buffer(config_triangle_buffer);
    std::memcpy(context.map_buffer(triangle_buffer), triangles.data(), sizeof(triangle_t) * triangles.size());

    gfx::config_buffer_t config_node_buffer{};
    config_node_buffer.vk_size = sizeof(node_t) * bvh.nodes.size();
    config_node_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_node_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t node_buffer = context.create_buffer(config_node_buffer);
    std::memcpy(context.map_buffer(node_buffer), bvh.nodes.data(), sizeof(node_t) * bvh.nodes.size());

    gfx::config_buffer_t config_primitive_index_buffer{};
    config_primitive_index_buffer.vk_size = sizeof(uint32_t) * bvh.primitive_indices.size();
    config_primitive_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_primitive_index_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t primitive_index_buffer = context.create_buffer(config_primitive_index_buffer);
    std::memcpy(context.map_buffer(primitive_index_buffer), bvh.primitive_indices.data(), sizeof(uint32_t) * bvh.primitive_indices.size());

    gfx::handle_descriptor_set_t debug_raytracing_inputs_descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = debug_raytracing_inputs_descriptor_set_layout });
    context.update_descriptor_set(debug_raytracing_inputs_descriptor_set)
        .push_buffer_write(0, gfx::buffer_descriptor_info_t{ .handle_buffer = triangle_buffer })
        .push_buffer_write(1, gfx::buffer_descriptor_info_t{ .handle_buffer = node_buffer })
        .push_buffer_write(2, gfx::buffer_descriptor_info_t{ .handle_buffer = primitive_index_buffer })
        .commit();

    gfx::config_buffer_t config_extra_buffer{};
    config_extra_buffer.vk_size = sizeof(glm::vec3);
    config_extra_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_extra_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    renderer::handle_buffer_t extra_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_extra_buffer);
    renderer::handle_descriptor_set_t extra_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = extra_descriptor_set_layout });
    renderer.update_descriptor_set(extra_descriptor_set)
        .push_buffer_write(0, { .handle_buffer = extra_buffer })
        .commit();

    gfx::config_buffer_t config_combined_triangle_buffer{};
    config_combined_triangle_buffer.vk_size = sizeof(triangle_t) * triangles.size();
    config_combined_triangle_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    gfx::handle_buffer_t combined_triangle_buffer = gfx::helper::create_and_push_buffer_staged(context, renderer.command_pool, config_combined_triangle_buffer, triangles.data());

    core::rng_t rng{};
    std::vector<object_t> objects{};
    for (auto& node : bvh.nodes) {
        transform_t t{};
        t.scale = (node.bounding_box.max - node.bounding_box.min) / 2.f;
        t.translation = (node.bounding_box.max + node.bounding_box.min) / 2.f;
        objects.push_back(new_object(context, object_descriptor_set_layout, rng, t));
    }

    editor_camera_t editor_camera{ window };
    core::frame_timer_t frame_timer{ 60.f };

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();
        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;

        auto dt = frame_timer.update();
        editor_camera.update(dt.count());

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        camera_ubo_t camera_ubo{};
        camera_ubo.projection = editor_camera.projection();
        camera_ubo.inv_projection = glm::inverse(camera_ubo.projection);
        camera_ubo.view = editor_camera.view();
        camera_ubo.inv_view = glm::inverse(camera_ubo.view);
        std::memcpy(context.map_buffer(renderer.buffer(camera_buffer)), &camera_ubo, sizeof(camera_ubo_t));

        glm::vec3 position = editor_camera.position();
        std::memcpy(context.map_buffer(renderer.buffer(extra_buffer)), &position, sizeof(glm::vec3));

        {
            gfx::rendering_attachment_t rendering_attachment{};
            rendering_attachment.clear_value = {0, 0, 0, 0};
            rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
            rendering_attachment.handle_image_view = target_image_view;

            context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            context.cmd_begin_rendering(commandbuffer, { rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
            
            // debug raytracing
            context.cmd_bind_pipeline(commandbuffer, debug_raytracing_pipeline);
            context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
            context.cmd_bind_descriptor_sets(commandbuffer, debug_raytracing_pipeline, 0, { debug_raytracing_inputs_descriptor_set, renderer.descriptor_set(camera_descriptor_set), renderer.descriptor_set(extra_descriptor_set) });
            context.cmd_draw(commandbuffer, 6, 1, 0, 0);

            // debug triangle
            context.cmd_bind_pipeline(commandbuffer, debug_triangle_pipeline);
            context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
            context.cmd_bind_descriptor_sets(commandbuffer, debug_triangle_pipeline, 0, { renderer.descriptor_set(camera_descriptor_set) });
            context.cmd_bind_vertex_buffers(commandbuffer, 0, { combined_triangle_buffer }, { 0 });
            context.cmd_draw(commandbuffer, triangles.size() * 3, 1, 0, 0);

            // debug node
            context.cmd_bind_pipeline(commandbuffer, debug_node_pipeline);
            context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
            context.cmd_bind_descriptor_sets(commandbuffer, debug_node_pipeline, 0, { renderer.descriptor_set(camera_descriptor_set) });
            for (auto& object : objects) {
                context.cmd_bind_descriptor_sets(commandbuffer, debug_node_pipeline, 1, { object.object_descriptor_set });
                for (auto& mesh : cube_meshes) {
                    context.cmd_bind_vertex_buffers(commandbuffer, 0, { mesh.vertex_buffer }, { 0 });
                    context.cmd_bind_index_buffer(commandbuffer, mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);
                    context.cmd_draw_indexed(commandbuffer, mesh.index_count, 1, 0, 0, 0);
                }
            }

            context.cmd_end_rendering(commandbuffer);
            context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        }

        renderer.end();
    }

    // core::window_t window{ "rt", 800, 800 };
    // gfx::context_t context{ true };

    // auto [width, height] = window.dimensions();

    // gfx::config_image_t config_target_image{};
    // config_target_image.vk_width = width;
    // config_target_image.vk_height = height;
    // config_target_image.vk_depth = 1;
    // config_target_image.vk_type = VK_IMAGE_TYPE_2D;
    // config_target_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    // config_target_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // config_target_image.vk_mips = 1;
    // gfx::handle_image_t target_image = context.create_image(config_target_image);
    // gfx::handle_image_view_t target_image_view = context.create_image_view({ .handle_image = target_image });

    // gfx::handle_sampler_t sampler = context.create_sampler({});

    // renderer::base_renderer_t renderer{ window, context, sampler, target_image_view };

    // gfx::config_descriptor_set_layout_t config_descriptor_set_layout{};
    // config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
    //                             .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
    //                             .add_layout_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    // gfx::handle_descriptor_set_layout_t descriptor_set_layout = context.create_descriptor_set_layout(config_descriptor_set_layout);

    // gfx::config_pipeline_layout_t config_raytracing_pipeline_layout{};
    // config_raytracing_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    // gfx::handle_pipeline_layout_t raytracing_pipeline_layout = context.create_pipeline_layout(config_raytracing_pipeline_layout);

    // gfx::config_pipeline_t config_raytracing_pipeline{};
    // config_raytracing_pipeline.handle_pipeline_layout = raytracing_pipeline_layout;
    // config_raytracing_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment())
    //                           .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/raytracing/glsl.vert").data(), .name = "raytracing vertex", .type = gfx::shader_type_t::e_vertex }))
    //                           .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/raytracing/glsl.frag").data(), .name = "raytracing fragment", .type = gfx::shader_type_t::e_fragment }));
    // gfx::handle_pipeline_t raytracing_pipeline = context.create_graphics_pipeline(config_raytracing_pipeline);

    // auto model = core::load_model_from_path("../../assets/models/triangle/triangle.obj");
    // std::vector<triangle_t> triangles;
    // {
    //     for (auto& mesh : model.meshes) {
    //         check(mesh.indices.size() % 3 == 0, "indices count is not a multiple of 3");
    //         for (size_t i = 0; i < mesh.indices.size(); i += 3) {
    //             core::vertex_t v0 = mesh.vertices[mesh.indices[i + 0]];
    //             core::vertex_t v1 = mesh.vertices[mesh.indices[i + 1]];
    //             core::vertex_t v2 = mesh.vertices[mesh.indices[i + 2]];

    //             triangle_t triangle;
    //             triangle.p0 = v0.position;
    //             triangle.p1 = v1.position;
    //             triangle.p2 = v2.position;

    //             triangles.push_back(triangle);
    //         }
    //     }
    // }
    // horizon_info("Loaded file with {} triangle(s)", triangles.size());
    // horizon_info("Triangles");
    // for (auto& triangle : triangles) {
    //     horizon_info("p0: {} p1: {} p2: {}", glm::to_string(triangle.p0), glm::to_string(triangle.p1), glm::to_string(triangle.p2));
    // }

    // std::vector<bounding_box_t> aabbs(triangles.size());
    // std::vector<glm::vec3> centers(triangles.size());
    
    // for (size_t i = 0; i < triangles.size(); i++) {
    //     aabbs[i] = bounding_box_t{}.extend(triangles[i].p0).extend(triangles[i].p1).extend(triangles[i].p2);
    //     centers[i] = (triangles[i].p0 + triangles[i].p1 + triangles[i].p2) / 3.0f;
    // }

    // bvh_t bvh = bvh_t::build(aabbs.data(), centers.data(), triangles.size());

    // horizon_info("Built BVH with {} node(s) and depth {}", bvh.nodes.size(), bvh.depth());
    // horizon_info("Nodes");
    // for (auto& node : bvh.nodes) {
    //     horizon_info("bb min: {}, max: {}, primitive_count: {}, first_index: {}", glm::to_string(node.bounding_box.min), glm::to_string(node.bounding_box.max), node.primitive_count, node.first_index);
    // }

    // horizon_info("Primitive Indices");
    // for (auto& id : bvh.primitive_indices) {
    //     horizon_info("{}", id);
    // }

    // gfx::config_buffer_t config_triangle_buffer{};
    // config_triangle_buffer.vk_size = sizeof(triangle_t) * triangles.size();
    // config_triangle_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    // config_triangle_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    // gfx::handle_buffer_t triangle_buffer = context.create_buffer(config_triangle_buffer);
    // std::memcpy(context.map_buffer(triangle_buffer), triangles.data(), sizeof(triangle_t) * triangles.size());

    // gfx::config_buffer_t config_node_buffer{};
    // config_node_buffer.vk_size = sizeof(node_t) * bvh.nodes.size();
    // config_node_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    // config_node_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    // gfx::handle_buffer_t node_buffer = context.create_buffer(config_node_buffer);
    // std::memcpy(context.map_buffer(node_buffer), bvh.nodes.data(), sizeof(node_t) * bvh.nodes.size());

    // gfx::config_buffer_t config_primitive_index_buffer{};
    // config_primitive_index_buffer.vk_size = sizeof(uint32_t) * bvh.primitive_indices.size();
    // config_primitive_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    // config_primitive_index_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    // gfx::handle_buffer_t primitive_index_buffer = context.create_buffer(config_primitive_index_buffer);
    // std::memcpy(context.map_buffer(primitive_index_buffer), bvh.primitive_indices.data(), sizeof(uint32_t) * bvh.primitive_indices.size());
    
    // gfx::handle_descriptor_set_t descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = descriptor_set_layout });
    // context.update_descriptor_set(descriptor_set)
    //     .push_buffer_write(0, gfx::buffer_descriptor_info_t{ .handle_buffer = triangle_buffer })
    //     .push_buffer_write(1, gfx::buffer_descriptor_info_t{ .handle_buffer = node_buffer })
    //     .push_buffer_write(2, gfx::buffer_descriptor_info_t{ .handle_buffer = primitive_index_buffer })
    //     .commit();

    // core::frame_timer_t frame_timer{ 60.f };
    // while (!window.should_close()) {
    //     core::clear_frame_function_times();
    //     core::window_t::poll_events();
    //     if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;

    //     renderer.begin();

    //     auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

    //     gfx::handle_commandbuffer_t commandbuffer = renderer.current_commandbuffer();
    //     {
    //         gfx::rendering_attachment_t rendering_attachment{};
    //         rendering_attachment.clear_value = { 0, 0, 0, 0 };
    //         rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    //         rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //         rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
    //         rendering_attachment.handle_image_view = target_image_view;

    //         context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    //         context.cmd_begin_rendering(commandbuffer, { rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
    //         context.cmd_bind_pipeline(commandbuffer, raytracing_pipeline);
    //         context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
    //         context.cmd_bind_descriptor_sets(commandbuffer, raytracing_pipeline, 0, { descriptor_set });
    //         context.cmd_draw(commandbuffer, 6, 1, 0, 0);
    //         context.cmd_end_rendering(commandbuffer);
    //         context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    //     }

    //     renderer.end();
    // }

    context.wait_idle();

    return 0;
}