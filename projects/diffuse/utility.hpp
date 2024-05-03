#ifndef UTILITY_HPP
#define UTILITY_HPP

#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/helper.hpp"

#include <glm/gtc/matrix_transform.hpp>

static gfx::handle_descriptor_set_layout_t material_descriptor_set_layout{};
static gfx::handle_sampler_t albedo_sampler;

void create_material_descriptor_set_layout(gfx::context_t& context) {
    gfx::config_descriptor_set_layout_t config_material{};
    config_material.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    material_descriptor_set_layout = context.create_descriptor_set_layout(config_material);
    albedo_sampler = context.create_sampler({});
}

void destroy_material_descriptor_set_layout(gfx::context_t& context) {
    context.destroy_descriptor_set_layout(material_descriptor_set_layout);
    context.destroy_sampler(albedo_sampler);
}

struct gpu_mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;

    gfx::handle_image_t albedo;
    gfx::handle_image_view_t albedo_view;
    gfx::handle_descriptor_set_t material_descriptor_set;
};

std::vector<gpu_mesh_t> load_model_from_path(gfx::context_t& context, gfx::handle_command_pool_t command_pool, const std::filesystem::path& model_path) {
    std::vector<gpu_mesh_t> gpu_meshes;
    core::model_t model = core::load_model_from_path(model_path);
    gfx::handle_fence_t fence = context.create_fence({});
    for (auto mesh : model.meshes) {
        gpu_mesh_t gpu_mesh{};
        gpu_mesh.vertex_count = mesh.vertices.size();
        gpu_mesh.index_count = mesh.indices.size();

        gfx::config_buffer_t config_vertex_buffer{};
        config_vertex_buffer.vk_size = mesh.vertices.size() * sizeof(core::vertex_t);
        config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        config_vertex_buffer.vma_allocation_create_flags = {};
        gpu_mesh.vertex_buffer = gfx::helper::create_and_push_buffer_staged(context, command_pool, config_vertex_buffer, mesh.vertices.data());

        gfx::config_buffer_t config_index_buffer{};
        config_index_buffer.vk_size = mesh.indices.size() * sizeof(uint32_t);
        config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        config_index_buffer.vma_allocation_create_flags = {};
        gpu_mesh.index_buffer = gfx::helper::create_and_push_buffer_staged(context, command_pool, config_index_buffer, mesh.indices.data());

        // gpu_mesh.albedo = 
        auto itr = std::find_if(mesh.material_description.texture_infos.begin(), mesh.material_description.texture_infos.end(), [](const core::texture_info_t& texture_info) {
            return texture_info.texture_type == core::texture_type_t::e_diffuse_map;
        });
        gpu_mesh.material_descriptor_set = context.allocate_descriptor_set(gfx::config_descriptor_set_t{.handle_descriptor_set_layout = material_descriptor_set_layout});
        // check(itr != mesh.material_description.texture_infos.end(), "mesh doesnt have diffuse texture ???");
        if (itr != mesh.material_description.texture_infos.end()) {
            gpu_mesh.albedo = gfx::helper::load_image_from_path(context, command_pool, itr->file_path, VK_FORMAT_R8G8B8A8_SRGB);
        } else {
            gpu_mesh.albedo = gfx::helper::load_image_from_path(context, command_pool, "../../assets/texture/default.png", VK_FORMAT_R8G8B8A8_SRGB);
        }

        gpu_mesh.albedo_view = context.create_image_view(gfx::config_image_view_t{.handle_image = gpu_mesh.albedo});
        context.update_descriptor_set(gpu_mesh.material_descriptor_set)
            .push_image_write(0, gfx::image_descriptor_info_t{.handle_sampler = albedo_sampler, .handle_image_view = gpu_mesh.albedo_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
            .commit();

        gpu_meshes.push_back(gpu_mesh);
    }
    return gpu_meshes;
}

class editor_camera_t {
public:
    editor_camera_t(core::window_t& window) : _window(window) {}
    void update_projection(float aspect_ratio);
    void update(float ts);
    
    const glm::mat4& projection() const { return _projection; }
    const glm::mat4& view() const { return _view; }
    // creates a new mat4
    glm::mat4 inverse_projection() const { return glm::inverse(_projection); }
    // creates a new mat4
    glm::mat4 inverse_view() const { return glm::inverse(_view); }
    glm::vec3 front() const { return _front; }
    glm::vec3 right() const { return _right; }
    glm::vec3 up() const { return _up; }
    glm::vec3 position() const { return _position; }

public:
    
    float fov{45.0f};  // zoom var ?
    float camera_speed_multiplyer{1};

    float far{1000.f};
    float near{0.1f};

private:
    core::window_t& _window;

    glm::vec3 _position {0, 0, 0};
    
    glm::mat4 _projection{1.0f};
    glm::mat4 _view{1.0f};

    glm::vec3 _front{0};
    glm::vec3 _up{0, 1, 0};
    glm::vec3 _right{0};

    glm::vec2 _initial_mouse{};

    float _yaw{0};
    float _pitch{0};
    float _mouse_speed{0.005f};
    float _mouse_sensitivity{100.f};
};

void editor_camera_t::update_projection(float aspect_ratio) {
    static float s_aspect_ratio = 0;
    if (s_aspect_ratio != aspect_ratio) {
        _projection = glm::perspective(glm::radians(fov), aspect_ratio, near, far);
        s_aspect_ratio = aspect_ratio;
    }
}

void editor_camera_t::update(float ts) {
    auto [width, height] = _window.dimensions();
    update_projection(float(width) / float(height));

    double curX, curY;
    glfwGetCursorPos(_window.window(), &curX, &curY);

    float velocity = _mouse_speed * ts;

    if (glfwGetKey(_window.window(), GLFW_KEY_W)) 
        _position += _front * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_S)) 
        _position -= _front * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_D)) 
        _position += _right * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_A)) 
        _position -= _right * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_SPACE)) 
        _position += _up * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_LEFT_SHIFT)) 
        _position -= _up * velocity;
    
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

    _view = glm::lookAt(_position, _position + _front, glm::vec3{0, 1, 0});
}

#endif