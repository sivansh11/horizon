#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"
#include "core/bvh.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static core::bvh_t::build_options_t build_options{
    .primitive_intersection_cost = 1.1f,
    .node_intersection_cost = 1.f,
    .min_primitive_count = 1,
    .max_primitive_count = std::numeric_limits<uint32_t>::max(),
    .sah_samples = 25,
    .add_node_intersection_cost_in_leaf_traversal = false,
};

std::pair<std::vector<core::triangle_t>, core::bvh_t> load_model(std::string path) {
    
    auto model = core::load_model_from_path(path);
    std::vector<core::triangle_t> tris;
    std::vector<core::aabb_t> aabbs;
    std::vector<core::vec3> centers;

    for (auto& mesh : model.meshes) {
        for (uint i = 0; i < mesh.indices.size(); i+=3) {
            core::triangle_t tri {
                mesh.vertices[mesh.indices[i + 0]],
                mesh.vertices[mesh.indices[i + 1]],
                mesh.vertices[mesh.indices[i + 2]],
            };
            core::aabb_t aabb{};
            aabb.grow(tri);
            core::vec3 center = tri.center();
            tris.push_back(tri);
            aabbs.push_back(aabb);
            centers.push_back(center);
        }
    }
    core::bvh_t bvh;
    if (std::filesystem::exists(path + ".bvh")) {
        horizon_warn("found cached bvh, loading that instead");
        bvh = core::bvh_t::load(path + ".bvh");
    } else {
        horizon_info("BUILDING BVH FROM SCRATCH");
        bvh = core::bvh_t::construct(aabbs.data(), centers.data(), tris.size(), build_options);
        bvh.to_disk(path + ".bvh");
    }

    return { tris, bvh };
}

struct uniform_t {
    VkDeviceAddress p_bvh;
    VkDeviceAddress p_triangle;
    glm::mat4 inverse_projection;
    glm::mat4 inverse_view;
};

struct gpu_node_t {
    core::vec3   min;
    uint32_t        primitive_count;
    core::vec3   max;
    uint32_t        first_index;
};

struct gpu_bvh_t {
    VkDeviceAddress p_nodes;
    VkDeviceAddress p_primitive_indices;
    uint32_t node_count;
    uint32_t primitive_count;
    VkDeviceAddress p_parent_ids;
};

struct blas_instance_t {
    VkDeviceAddress p_bvh;
    VkDeviceAddress p_triangle;
    glm::mat4 inverse_transform;
};

VkDeviceAddress to_gpu(renderer::base_renderer_t& renderer, const void *p_data, size_t n) {
    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = n;
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    gfx::handle_buffer_t buffer = renderer.context.create_buffer(config_buffer);

    gfx::config_buffer_t config_staging_buffer{};
    config_staging_buffer.vk_size = n;
    config_staging_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    config_staging_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t staging_buffer = renderer.context.create_buffer(config_staging_buffer);

    std::memcpy(renderer.context.map_buffer(staging_buffer), p_data, n);

    gfx::handle_commandbuffer_t commandbuffer = gfx::helper::start_single_use_commandbuffer(renderer.context, renderer.command_pool);
    renderer.context.cmd_copy_buffer(commandbuffer, staging_buffer, buffer, { .vk_size = config_buffer.vk_size });
    gfx::helper::end_single_use_commandbuffer(renderer.context, commandbuffer);

    renderer.context.destroy_buffer(staging_buffer);
    
    return renderer.context.get_buffer_device_address(buffer);
}

VkDeviceAddress to_gpu(renderer::base_renderer_t& renderer, const core::bvh_t& bvh) {
    gpu_bvh_t gpu_bvh;
    gpu_bvh.node_count = bvh.node_count;
    gpu_bvh.primitive_count = bvh.primitive_count;
    gpu_bvh.p_nodes = to_gpu(renderer, bvh.p_nodes, sizeof(core::node_t) * bvh.node_count);
    gpu_bvh.p_primitive_indices = to_gpu(renderer, bvh.p_primitive_indices, sizeof(uint32_t) * bvh.primitive_count);
    gpu_bvh.p_parent_ids = to_gpu(renderer, bvh.p_parent_ids, sizeof(uint32_t) * bvh.node_count);
    return to_gpu(renderer, &gpu_bvh, sizeof(gpu_bvh));
}

VkDeviceAddress to_gpu(renderer::base_renderer_t& renderer, const std::vector<core::triangle_t>& tris) {
    return to_gpu(renderer, tris.data(), tris.size() * sizeof(core::triangle_t));
}

blas_instance_t create_blas_instance(renderer::base_renderer_t& renderer, const core::bvh_t& bvh, const std::vector<core::triangle_t>& tris, const glm::mat4 transform) {
    blas_instance_t blas_instance;

    blas_instance.p_bvh = to_gpu(renderer, bvh);
    blas_instance.p_triangle = to_gpu(renderer, tris);
    blas_instance.inverse_transform = glm::inverse(transform);

    return blas_instance;
}

VkDeviceAddress to_gpu(renderer::base_renderer_t& renderer, const std::vector<blas_instance_t>& blas_instances) {
    return to_gpu(renderer, blas_instances.data(), blas_instances.size() * sizeof(blas_instance_t));
}

void show_bvh_info(const core::bvh_t& bvh) {
    uint32_t max = 0;
    uint32_t min = std::numeric_limits<uint32_t>::max();
    float average = 0;
    uint32_t leaf_node_count = 0;
    for (uint32_t i = 0; i < bvh.node_count; i++) {
        auto& node = bvh.p_nodes[i];
        if (node.is_leaf()) {
            if (node.primitive_count > max) {
                max = node.primitive_count;
            }
            if (node.primitive_count < min) {
                min = node.primitive_count;
            }
            average += node.primitive_count;
            leaf_node_count++;
        }
    }

    horizon_info("\e[0;97mnode count: {}", bvh.node_count);
    horizon_info("\e[0;97mdepth: {}", bvh.depth());
    horizon_info("\e[0;97mglobal sah cost: {}", bvh.node_traversal_cost(build_options));
    horizon_info("\e[0;97mleaf count: {}", leaf_node_count);
    horizon_info("\e[0;97mmax tris per leaf: {}", max);
    horizon_info("\e[0;97mmin tris per leaf: {}", min);
    horizon_info("\e[0;97maverage tris per leaf: {}", average / float(leaf_node_count));
}

int main() {
    // core::log_t::set_log_level(core::log_level_t::e_info);
    
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

    // auto [tris, bvh] = load_model("../../assets/models/Bistro_v5_2/BistroExterior.fbx");
    auto [tris, bvh] = load_model("../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
    show_bvh_info(bvh);

    auto p_bvh = to_gpu(renderer, bvh);
    auto p_tris = to_gpu(renderer, tris);

    gfx::config_buffer_t config_uniform_buffer{};
    config_uniform_buffer.vk_size = sizeof(uniform_t);
    config_uniform_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_uniform_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    renderer::handle_buffer_t uniform_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_uniform_buffer);

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
        if (glfwGetKey(window.window(), GLFW_KEY_RIGHT_SHIFT)) {
            camera.camera_speed_multiplyer = 100;
        } else {
            camera.camera_speed_multiplyer = 1;   
        }
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

        uniform->p_bvh = p_bvh;
        uniform->p_triangle = p_tris;
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