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

std::pair<std::vector<horizon::triangle_t>, horizon::bvh_t> load_bbk_model_and_bvh() {
    core::binary_reader_t vertex_reader{ "../../assets/models/bbk_sponza_and_bvh/sah-samples-25/MergedSponza (1)/MergedSponza.vertices" };
    struct vertex_t {
        horizon::vec3 positon;
    };
    size_t vertex_list_size = vertex_reader.file_size() / sizeof(vertex_t);
    std::vector<vertex_t> vertex_list;
    for (uint32_t i = 0; i < vertex_list_size; i++) {
        vertex_t vertex;
        vertex_reader.read(vertex);
        vertex_list.push_back(vertex);
    }
    horizon_info("{}", vertex_list_size);

    core::binary_reader_t tri_indices_reader{ "../../assets/models/bbk_sponza_and_bvh/sah-samples-25/MergedSponza (1)/MergedSponza.triIndices" };
    struct tri_indices_t {
        uint32_t i0, i1, i2;
    };
    size_t tri_indices_list_size = tri_indices_reader.file_size() / sizeof(tri_indices_t);
    std::vector<tri_indices_t> tri_indices_list;
    for (uint32_t i = 0; i < tri_indices_list_size; i++) {
        tri_indices_t tri_indices;
        tri_indices_reader.read(tri_indices);
        tri_indices_list.push_back(tri_indices);
    }
    horizon_info("{}", tri_indices_list_size);
    
    auto transform = glm::scale(glm::mat4{1.f}, glm::vec3{0.01, 0.01, 0.01});

    core::binary_reader_t node_reader{ "../../assets/models/bbk_sponza_and_bvh/sah-samples-25/MergedSponza (1)/MergedSponza.nodes" };
    size_t node_list_size = node_reader.file_size() / sizeof(horizon::node_t);
    std::vector<horizon::node_t> node_list;
    for (uint32_t i = 0; i < node_list_size; i++) {
        horizon::node_t node;
        node_reader.read(node);
        node.min = transform * glm::vec4(node.min, 1);
        node.max = transform * glm::vec4(node.max, 1);
        node_list.push_back(node);
    }
    horizon_info("{}", node_list_size);

    std::vector<horizon::triangle_t> tris{};
    for (auto& tri_indices : tri_indices_list) {
        horizon::triangle_t tri {
            transform * glm::vec4(vertex_list[tri_indices.i0].positon, 1),
            transform * glm::vec4(vertex_list[tri_indices.i1].positon, 1),
            transform * glm::vec4(vertex_list[tri_indices.i2].positon, 1),
        };
        tris.push_back(tri);
    }

    horizon::bvh_t bvh{};
    bvh.primitive_count = tris.size();
    bvh.p_primitive_indices = new uint32_t[bvh.primitive_count];
    for (uint32_t i = 0; i < bvh.primitive_count; i++) bvh.p_primitive_indices[i] = i;
    bvh.node_count = node_list.size();
    bvh.p_nodes = new horizon::node_t[bvh.node_count];
    for (uint32_t i = 0; i < bvh.node_count; i++) {
        bvh.p_nodes[i] = node_list[i];
    }
    bvh.p_parent_ids = new uint32_t[bvh.node_count];

    return { tris, bvh };
}

void save_model_for_bbk(std::string path, const horizon::bvh_t& bvh, const core::model_t& model) {
    core::binary_writer_t writer{ path + ".compressed_triangle_and_bvh" };

    struct vertex_t {
        glm::vec3 position;
    };
    std::vector<vertex_t> vertex_list;
    for (auto& mesh : model.meshes) {
        for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
            vertex_t vertex = { mesh.vertices[i].position };
            vertex_list.push_back(vertex);
        }
    }
    uint32_t vertex_count = vertex_list.size();
    writer.write(vertex_count);
    // horizon_info("{}", vertex_count);
    for (auto& vertex : vertex_list) {
        writer.write(vertex);
        // horizon_info("{}", glm::to_string(vertex.position));
    }

    std::vector<uint32_t> indices;
    uint32_t offset = 0;
    for (auto& mesh : model.meshes) {
        for (uint32_t i = 0; i < mesh.indices.size(); i++) {
            indices.push_back(offset + mesh.indices[i]);
        }
        offset += mesh.vertices.size();
    }
    uint32_t index_count = indices.size();
    writer.write(index_count);
    // horizon_info("{}", index_count);
    for (auto& index : indices) {
        writer.write(index);
        // horizon_info("{}", index);
    }

    uint32_t node_count = bvh.node_count;
    writer.write(node_count);
    // horizon_info("{}", node_count);
    for (uint32_t i = 0; i < node_count; i++) {
        writer.write(bvh.p_nodes[i]);
        // horizon_info("{} {} {} {}", glm::to_string(bvh.p_nodes[i].min), bvh.p_nodes[i].first_index, glm::to_string(bvh.p_nodes[i].max), bvh.p_nodes[i].primitive_count);
    }

    uint32_t primitive_indices_count = bvh.primitive_count;
    writer.write(primitive_indices_count);
    // horizon_info("{}", primitive_indices_count);
    for (uint32_t i = 0; i < primitive_indices_count; i++) {
        writer.write(bvh.p_primitive_indices[i]);
        // horizon_info("{}", bvh.p_primitive_indices[i]);
    }
}

std::pair<std::vector<horizon::triangle_t>, horizon::bvh_t> load_my_model() {
    core::binary_reader_t reader{ "../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf.compressed_triangle_and_bvh" };

    horizon::bvh_t bvh;

    struct vertex_t {
        glm::vec3 position;
    };
    std::vector<vertex_t> vertex_list{};
    uint32_t vertex_count;
    reader.read(vertex_count);
    horizon_info("{}", vertex_count);
    for (uint32_t i = 0; i < vertex_count; i++) {
        vertex_t vertex;
        reader.read(vertex);
        horizon_info("{}", glm::to_string(vertex.position));
        vertex_list.push_back(vertex);
    }

    std::vector<uint32_t> index_list{};
    uint32_t index_count;
    reader.read(index_count);
    horizon_info("{}", index_count);
    for (uint32_t i = 0; i < index_count; i++) {
        uint32_t index;
        reader.read(index);
        horizon_info("{}", index);
        index_list.push_back(index);
    }

    reader.read(bvh.node_count);
    horizon_info("{}", bvh.node_count);
    bvh.p_nodes = new horizon::node_t[bvh.node_count];
    for (uint32_t i = 0; i < bvh.node_count; i++) {
        reader.read(bvh.p_nodes[i]);
        horizon_info("{} {} {} {}", glm::to_string(bvh.p_nodes[i].min), bvh.p_nodes[i].first_index, glm::to_string(bvh.p_nodes[i].max), bvh.p_nodes[i].primitive_count);
    }

    reader.read(bvh.primitive_count);
    horizon_info("{}", bvh.primitive_count);
    bvh.p_primitive_indices = new uint32_t[bvh.primitive_count];
    for (uint32_t i = 0; i < bvh.primitive_count; i++) {
        reader.read(bvh.p_primitive_indices[i]);
        horizon_info("{}", bvh.p_primitive_indices[i]);
    }

    std::vector<horizon::triangle_t> tris;
    for (uint32_t i = 0; i < index_list.size(); i+=3) {
        horizon::triangle_t tri {
            vertex_list[index_list[i + 0]].position,
            vertex_list[index_list[i + 1]].position,
            vertex_list[index_list[i + 2]].position,
        };
        tris.push_back(tri);
    }

    return { tris, bvh };
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


    horizon::bvh_t::build_options_t build_options{
        .primitive_intersection_cost = 1.1f,
        .node_intersection_cost = 1.f,
        .min_primitive_count = 1,
        .max_primitive_count = std::numeric_limits<uint32_t>::max(),
        .add_node_intersection_cost_in_leaf_traversal = false,
    };
    
    std::string model_path = "../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf";
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
    horizon::bvh_t bvh;
    if (std::filesystem::exists(model_path + ".bvh") && false) {
        horizon_warn("found cached bvh, loading that instead");
        bvh = horizon::bvh_t::load(model_path + ".bvh");
    } else {
        bvh = horizon::bvh_t::construct(aabbs.data(), centers.data(), tris.size(), build_options);
        horizon_info("BUILD BVH FROM SCRATCH");
        bvh.to_disk(model_path + ".bvh");
    }
    save_model_for_bbk(model_path, bvh, model);

    // auto [tris, bvh] = load_bbk_model_and_bvh();

    // auto [tris, bvh] = load_my_model();
    // bvh.p_parent_ids = new uint32_t[bvh.primitive_count];

    // horizon_info("bvh depth: {} node count: {}", bvh.depth(), bvh.node_count);
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

    // horizon_info("max triangles per leaf: {} average triangles per leaf: {} leaf node count: {}", max, average / float(leaf_node_count), leaf_node_count);

    horizon_info("\e[0;97mprimitive count: {}", tris.size());
    horizon_info("\e[0;97mnode count: {}", bvh.node_count);
    horizon_info("\e[0;97mdepth: {}", bvh.depth());
    horizon_info("\e[0;97mglobal sah cost: {}", bvh.node_traversal_cost(build_options));
    horizon_info("\e[0;97mleaf count: {}", leaf_node_count);
    horizon_info("\e[0;97mmax tris per leaf: {}", max);
    horizon_info("\e[0;97mmin tris per leaf: {}", min);
    horizon_info("\e[0;97maverage tris per leaf: {}", average / float(leaf_node_count));
    

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