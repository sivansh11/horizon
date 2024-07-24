#include "core/core.hpp"
#include "core/window.hpp"
#include "core/components.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include "utility.hpp"

class editor_camera_t : public core::camera_t {
public:
    editor_camera_t(core::window_t& window) : _window(window) {
        auto [width, height] = _window.dimensions();
        update_projection(float(width) / float(height));
    }

    void update_projection(float aspect_ratio) {
        static float s_aspect_ratio = 0;
        if (s_aspect_ratio != aspect_ratio) {
            _projection = glm::perspective(glm::radians(fov), aspect_ratio, near, far);
            s_aspect_ratio = aspect_ratio;
        }
    }

    void update(float dt) {
        auto [width, height] = _window.dimensions();
        update_projection(float(width) / float(height));

        double curX, curY;
        glfwGetCursorPos(_window.window(), &curX, &curY);

        float velocity = _mouse_speed * dt;

        glm::vec3 position = core::camera_t::position();

        if (glfwGetKey(_window.window(), GLFW_KEY_W)) 
            position += _front * velocity;
        if (glfwGetKey(_window.window(), GLFW_KEY_S)) 
            position -= _front * velocity;
        if (glfwGetKey(_window.window(), GLFW_KEY_D)) 
            position += _right * velocity;
        if (glfwGetKey(_window.window(), GLFW_KEY_A)) 
            position -= _right * velocity;
        if (glfwGetKey(_window.window(), GLFW_KEY_SPACE)) 
            position += _up * velocity;
        if (glfwGetKey(_window.window(), GLFW_KEY_LEFT_SHIFT)) 
            position -= _up * velocity;
        
        glm::vec2 mouse{curX, curY};
        glm::vec2 difference = mouse - _initial_mouse;
        _initial_mouse = mouse;

        if (glfwGetMouseButton(_window.window(), GLFW_MOUSE_BUTTON_1)) {
            
            difference.x = difference.x / float(width);
            difference.y = -(difference.y / float(height));

            _yaw += difference.x * _mouse_sensitivity;
            _pitch += difference.y * _mouse_sensitivity;

            if (_pitch > 89.0f) 
                _pitch = 89.0f;
            if (_pitch < -89.0f) 
                _pitch = -89.0f;
        }

        glm::vec3 front;
        front.x = glm::cos(glm::radians(_yaw)) * glm::cos(glm::radians(_pitch));
        front.y = glm::sin(glm::radians(_pitch));
        front.z = glm::sin(glm::radians(_yaw)) * glm::cos(glm::radians(_pitch));
        _front = front * camera_speed_multiplyer;
        _right = glm::normalize(glm::cross(_front, glm::vec3{0, 1, 0}));
        _up    = glm::normalize(glm::cross(_right, _front));

        _view = glm::lookAt(position, position + _front, glm::vec3{0, 1, 0});

        core::camera_t::update();
    }

    float fov{ 45.0f };
    float camera_speed_multiplyer{ 1.0f };
    float far{ 1000.0f };
    float near{ 0.1f };

private:
    core::window_t& _window;

    glm::vec3 _front{ 0.0f };
    glm::vec3 _up{ 0.0f, 1.0f, 0.0f };
    glm::vec3 _right{ 0.0f };

    glm::vec2 _initial_mouse{};

    float _yaw{ 0.0f };
    float _pitch{ 0.0f };
    float _mouse_speed{ 0.005f };
    float _mouse_sensitivity{ 100.0f };
};

struct camera_uniform_t {
    glm::mat4 projection;
    glm::mat4 inv_projection;
    glm::mat4 view;
    glm::mat4 inv_view;
};

struct object_uniform_t {
    glm::mat4 model;
    glm::mat4 inv_model;
};

struct gpu_mesh_t {
    
};

int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "basic", 1200, 800 };
    gfx::context_t context{ true };

    auto [width, height] = window.dimensions();
    auto [target_image, target_image_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto [target_depth_image, target_depth_image_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    gfx::handle_sampler_t sampler = context.create_sampler({});

    renderer::base_renderer_t renderer{ window, context, sampler, target_image_view };
    
    gfx::config_descriptor_set_layout_t camera_config_descriptor_set_layout{};
    camera_config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    gfx::handle_descriptor_set_layout_t camera_descriptor_set_layout = context.create_descriptor_set_layout(camera_config_descriptor_set_layout);

    gfx::config_descriptor_set_layout_t object_config_descriptor_set_layout{};
    object_config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    gfx::handle_descriptor_set_layout_t object_descriptor_set_layout = context.create_descriptor_set_layout(object_config_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(camera_descriptor_set_layout);
    config_pipeline_layout.add_descriptor_set_layout(object_descriptor_set_layout);
    gfx::handle_pipeline_layout_t pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);

    gfx::config_pipeline_t predepth_config_pipeline{};
    predepth_config_pipeline.handle_pipeline_layout = pipeline_layout;
    predepth_config_pipeline.set_depth_attachment(VK_FORMAT_D32_SFLOAT, VkPipelineDepthStencilStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .stencilTestEnable = VK_FALSE,
    });
    predepth_config_pipeline.add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    predepth_config_pipeline.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position));
    predepth_config_pipeline.add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, normal));
    predepth_config_pipeline.add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv));
    predepth_config_pipeline.add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, tangent));
    predepth_config_pipeline.add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, bi_tangent));
    predepth_config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/predepth/glsl.vert", .is_code = false, .name = "predepth vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_glsl }));
    predepth_config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/predepth/glsl.frag", .is_code = false, .name = "predepth fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_glsl }));
    gfx::handle_pipeline_t predepth_pipeline = context.create_graphics_pipeline(predepth_config_pipeline);

    renderer::handle_descriptor_set_t camera_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = camera_descriptor_set_layout });

    gfx::config_buffer_t config_camera_buffer{};
    config_camera_buffer.vk_size = sizeof(camera_uniform_t);
    config_camera_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_camera_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    renderer::handle_buffer_t camera_uniform_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_camera_buffer);

    renderer.update_descriptor_set(camera_descriptor_set)
        .push_buffer_write(0, { .handle_buffer = camera_uniform_buffer })
        .commit();

    editor_camera_t camera{ window }; 
    core::frame_timer_t frame_timer{  60.f };

    while (!window.should_close()) {
        core::window_t::poll_events();

        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;

        core::timer::duration_t dt = frame_timer.update();
        camera.update(dt.count());

        renderer.begin();
        gfx::handle_commandbuffer_t commandbuffer = renderer.current_commandbuffer();

        camera_uniform_t camera_uniform{};
        camera_uniform.projection = camera.projection();
        camera_uniform.inv_projection = camera.inv_projection();
        camera_uniform.view = camera.view();
        camera_uniform.inv_view = camera.inv_view();
        std::memcpy(context.map_buffer(renderer.buffer(camera_uniform_buffer)), &camera_uniform, sizeof(camera_uniform_t));

        gfx::rendering_attachment_t color_rendering_attachment{};
        color_rendering_attachment.clear_value = { 0, 0, 0, 0 };
        color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        color_rendering_attachment.handle_image_view = target_image_view;

        gfx::rendering_attachment_t depth_rendering_attachment{};
        depth_rendering_attachment.clear_value.depthStencil.depth = 1;
        depth_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        depth_rendering_attachment.handle_image_view = target_depth_image_view;

        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_image_memory_barrier(commandbuffer, target_depth_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        context.cmd_begin_rendering(commandbuffer, { color_rendering_attachment }, depth_rendering_attachment, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }});

        context.cmd_end_rendering(commandbuffer);

        renderer.end();
    }

    return 0;
}