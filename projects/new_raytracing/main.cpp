#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"
#include "bvh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct gpu_node_t {
    horizon::vec3   min;
    uint32_t        primitive_count;
    horizon::vec3   max;
    uint32_t        first_index;
};

struct gpu_bvh_data_t {
    VkDeviceAddress p_gpu_node;
    VkDeviceAddress p_gpu_primitive_indices;
    VkDeviceAddress p_gpu_parent_ids;
    uint32_t node_count;
    uint32_t primitive_count;
};

struct uniform_t {
    VkDeviceAddress gpu_bvh_bda;
    VkDeviceAddress gpu_triangle_bda;
    glm::mat4 inverse_projection;
    glm::mat4 inverse_view;
};

gfx::handle_buffer_t create_and_push_n_bits_to_dedicated_memory_buffer(gfx::context_t& context, gfx::handle_command_pool_t& handle_command_pool, void *p_data, size_t n) {
    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = n;
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    gfx::handle_buffer_t buffer = context.create_buffer(config_buffer);

    gfx::config_buffer_t config_staging_buffer{};
    config_staging_buffer.vk_size = n;
    config_staging_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    config_staging_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t staging_buffer = context.create_buffer(config_staging_buffer);

    std::memcpy(context.map_buffer(staging_buffer), p_data, n);

    gfx::handle_commandbuffer_t commandbuffer = gfx::helper::start_single_use_commandbuffer(context, handle_command_pool);
    context.cmd_copy_buffer(commandbuffer, staging_buffer, buffer, { .vk_size = config_buffer.vk_size });
    gfx::helper::end_single_use_commandbuffer(context, commandbuffer);

    context.destroy_buffer(staging_buffer);

    return buffer;
}

int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);
    
    core::window_t window{ "this_app", 800, 800 };
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

    gfx::config_descriptor_set_layout_t config_rt_descriptor_set_layout{};
    config_rt_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t rt_descriptor_set_layout = context.create_descriptor_set_layout(config_rt_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_rt_pipeline_layout{};
    config_rt_pipeline_layout.add_descriptor_set_layout(rt_descriptor_set_layout);
    gfx::handle_pipeline_layout_t rt_pipeline_layout = context.create_pipeline_layout(config_rt_pipeline_layout);

    gfx::config_pipeline_t config_rt_pipeline{};
    config_rt_pipeline.handle_pipeline_layout = rt_pipeline_layout;
    config_rt_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment())
                                    .add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/raytracing/vert.slang", .name = "raytracing vertex", .type = gfx::shader_type_t::e_vertex }))
                                    .add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/raytracing/frag.slang", .name = "raytracing fragment", .type = gfx::shader_type_t::e_fragment }));
    gfx::handle_pipeline_t rt_pipeline = context.create_graphics_pipeline(config_rt_pipeline);

    std::string model_path = "../../assets/models/sponza_bbk/SponzaMerged/SponzaMerged.obj";
    auto model = core::load_model_from_path(model_path);
    std::vector<horizon::triangle_t> tris;
    std::vector<horizon::aabb_t> aabbs;
    std::vector<horizon::vec3> centers;

    for (auto& mesh : model.meshes) {
        for (uint i = 0; i < mesh.indices.size(); i+=3) {
            horizon::triangle_t tri {
                mesh.vertices[mesh.indices[i + 0]].position,
                mesh.vertices[mesh.indices[i + 1]].position,
                mesh.vertices[mesh.indices[i + 2]].position,
            };
            horizon::aabb_t aabb{};
            aabb.grow(tri);
            horizon::vec3 center = tri.center();
            tris.push_back(tri);
            aabbs.push_back(aabb);
            centers.push_back(center);
        }
    }

    horizon::bvh_t::build_options_t build_options{
        .primitive_intersection_cost = 1.1f,
        .node_intersection_cost = 1.f,
        .min_primitive_count = 1,
        .max_primitive_count = std::numeric_limits<uint32_t>::max(),
        .add_node_intersection_cost_in_leaf_traversal = false,
    };
    horizon::bvh_t bvh;
    if (std::filesystem::exists(model_path + ".bvh")) {
        horizon_warn("found cached bvh, loading that instead");
        bvh = horizon::bvh_t::load(model_path + ".bvh");
    } else {
        bvh = horizon::bvh_t::construct(aabbs.data(), centers.data(), tris.size(), build_options);
        std::cout << "built bvh with " << bvh.node_count << " nodes.\n";
        bvh.to_disk(model_path + ".bvh");
    }

    horizon_info("bvh depth: {} node count: {}", bvh.depth(), bvh.node_count);
    uint32_t max = 0;
    float average = 0;
    uint32_t leaf_node_count = 0;
    for (uint32_t i = 0; i < bvh.node_count; i++) {
        auto& node = bvh.p_nodes[i];
        if (node.is_leaf()) {
            if (node.primitive_count > max) {
                max = node.primitive_count;
            }
            average += node.primitive_count;
            leaf_node_count++;
        }
    }

    horizon_info("max triangles per leaf: {} average triangles per leaf: {} leaf node count: {}", max, average / float(leaf_node_count), leaf_node_count);
    

    gfx::handle_buffer_t bvh_nodes = create_and_push_n_bits_to_dedicated_memory_buffer(context, renderer.command_pool, bvh.p_nodes, bvh.node_count * sizeof(horizon::node_t));
    gfx::handle_buffer_t bvh_primitive_indices = create_and_push_n_bits_to_dedicated_memory_buffer(context, renderer.command_pool, bvh.p_primitive_indices, bvh.primitive_count * sizeof(uint32_t));
    gfx::handle_buffer_t bvh_parent_ids = create_and_push_n_bits_to_dedicated_memory_buffer(context, renderer.command_pool, bvh.p_parent_ids, bvh.node_count * sizeof(uint32_t));

    gpu_bvh_data_t gpu_bvh_data;
    gpu_bvh_data.node_count = bvh.node_count;
    gpu_bvh_data.primitive_count = bvh.primitive_count;
    gpu_bvh_data.p_gpu_node = context.get_buffer_device_address(bvh_nodes);
    gpu_bvh_data.p_gpu_primitive_indices = context.get_buffer_device_address(bvh_primitive_indices);
    gpu_bvh_data.p_gpu_parent_ids = context.get_buffer_device_address(bvh_parent_ids);

    gfx::handle_buffer_t gpu_bvh = create_and_push_n_bits_to_dedicated_memory_buffer(context, renderer.command_pool, &gpu_bvh_data, sizeof(gpu_bvh_data_t));
    gfx::handle_buffer_t gpu_triangles = create_and_push_n_bits_to_dedicated_memory_buffer(context, renderer.command_pool, tris.data(), tris.size() * sizeof(horizon::triangle_t));

    gfx::config_buffer_t config_uniform_buffer{};
    config_uniform_buffer.vk_size = sizeof(uniform_t);
    config_uniform_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_uniform_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    renderer::handle_buffer_t uniform_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_uniform_buffer);

    // uniform_t *uniform = reinterpret_cast<uniform_t *>(context.map_buffer(uniform_buffer));

    // uniform->gpu_bvh_bda = context.get_buffer_device_address(gpu_bvh);
    // uniform->gpu_triangle_bda = context.get_buffer_device_address(gpu_triangles);

    // gfx::handle_descriptor_set_t rt_descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = rt_descriptor_set_layout });
    renderer::handle_descriptor_set_t rt_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = rt_descriptor_set_layout });
    renderer.update_descriptor_set(rt_descriptor_set) 
        .push_buffer_write(0, { .handle_buffer = uniform_buffer })
        .commit();

    editor_camera_t camera{ window };

    float target_fps = 60.f;
    auto last_time = std::chrono::system_clock::now();

    core::frame_timer_t frame_timer{ 60.f };

    gfx::handle_timer_t timer = context.create_timer({});

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();
        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;
        if (glfwGetKey(window.window(), GLFW_KEY_R)) {
            context.wait_idle();
            context.destroy_pipeline(rt_pipeline);
            gfx::config_pipeline_t config_rt_pipeline{};
            config_rt_pipeline.handle_pipeline_layout = rt_pipeline_layout;
            config_rt_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment())
                                            .add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/raytracing/vert.slang", .name = "raytracing vertex", .type = gfx::shader_type_t::e_vertex }))
                                            .add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/raytracing/frag.slang", .name = "raytracing fragment", .type = gfx::shader_type_t::e_fragment }));
            rt_pipeline = context.create_graphics_pipeline(config_rt_pipeline);

        }

        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_fps) {
            continue;
        }
        last_time = current_time;

        auto dt = frame_timer.update();
        camera.update(dt.count());

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        gfx::rendering_attachment_t rendering_attachment{};
        rendering_attachment.clear_value = {0, 0, 0, 0};
        rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        rendering_attachment.handle_image_view = target_image_view;

        uniform_t *uniform = reinterpret_cast<uniform_t *>(context.map_buffer(renderer.buffer(uniform_buffer)));

        uniform->gpu_bvh_bda = context.get_buffer_device_address(gpu_bvh);
        uniform->gpu_triangle_bda = context.get_buffer_device_address(gpu_triangles);
        uniform->inverse_projection = camera.inverse_projection();
        uniform->inverse_view = camera.inverse_view();

        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_begin_timer(commandbuffer, timer);
        context.cmd_begin_rendering(commandbuffer, { rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        context.cmd_bind_pipeline(commandbuffer, rt_pipeline);
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_bind_descriptor_sets(commandbuffer, rt_pipeline, 0, { renderer.descriptor_set(rt_descriptor_set) });
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);
        context.cmd_end_timer(commandbuffer, timer);

        static uint frame_count = 0;
        frame_count++;
        if (frame_count > 0) {
            frame_count = 0;
            if (auto t = context.timer_get_time(timer)) {
                horizon_info("{} {}", *t, dt.count());
            } else {
                horizon_info("");
            }
        }

        context.cmd_end_rendering(commandbuffer);
        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);


        renderer.end();
    }

    context.wait_idle();

    return 0;
}