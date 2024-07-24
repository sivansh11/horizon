#ifndef UTILITY_HPP
#define UTILITY_HPP

#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/helper.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>

#include "bvh.hpp"

// std::tuple<std::vector<triangle_t>, std::vector<bvh_t>> create_blas_from_model(const core::model_t& model) {
    
// }

struct transform_t {
    core::vec3 translation{ 0.f, 0.f, 0.f };
    core::vec3 rotation{ 0.f, 0.f, 0.f };
    core::vec3 scale{ 1.f, 1.f, 1.f };
    
    transform_t() = default;

    core::mat4 mat4() const {
        core::mat4 transform = core::translate(core::mat4(1.f), translation);
        transform = core::rotate(transform, rotation.x, core::vec3(1.0f, 0.0f, 0.0f));
        transform = core::rotate(transform, rotation.y, core::vec3(0.0f, 1.0f, 0.0f));
        transform = core::rotate(transform, rotation.z, core::vec3(0.0f, 0.0f, 1.0f));
        transform = core::scale(transform, scale);
        return transform;
    }
};

class editor_camera_t {
public:
    editor_camera_t(core::window_t& window) : _window(window) {}
    void update_projection(float aspect_ratio);
    void update(float ts);
    
    const core::mat4& projection() const { return _projection; }
    const core::mat4& view() const { return _view; }
    // creates a new mat4
    core::mat4 inverse_projection() const { return core::inverse(_projection); }
    // creates a new mat4
    core::mat4 inverse_view() const { return core::inverse(_view); }
    core::vec3 front() const { return _front; }
    core::vec3 right() const { return _right; }
    core::vec3 up() const { return _up; }
    core::vec3 position() const { return _position; }

public:
    
    float fov{90.0f};  // zoom var ?
    float camera_speed_multiplyer{1};

    float far{1000.f};
    float near{0.1f};

private:
    core::window_t& _window;

    core::vec3 _position {0, 2, 0};
    
    core::mat4 _projection{1.0f};
    core::mat4 _view{1.0f};

    core::vec3 _front{1, 0, 0};
    core::vec3 _up{0, 1, 0};
    core::vec3 _right{1, 0, 0};

    core::vec2 _initial_mouse{};

    float _yaw{0};
    float _pitch{0};
    float _mouse_speed{0.005f};
    float _mouse_sensitivity{100.f};
};

void editor_camera_t::update_projection(float aspect_ratio) {
    static float s_aspect_ratio = 0;
    if (s_aspect_ratio != aspect_ratio) {
        _projection = core::perspective(core::radians(fov), aspect_ratio, near, far);
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
    
    core::vec2 mouse{curX, curY};
    core::vec2 difference = mouse - _initial_mouse;
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

    core::vec3 front;
    front.x = core::cos(core::radians(_yaw)) * core::cos(core::radians(_pitch));
    front.y = core::sin(core::radians(_pitch));
    front.z = core::sin(core::radians(_yaw)) * core::cos(core::radians(_pitch));
    _front = front * camera_speed_multiplyer;
    _right = core::normalize(core::cross(_front, core::vec3{0, 1, 0}));
    _up    = core::normalize(core::cross(_right, _front));

    _view = core::lookAt(_position, _position + _front, core::vec3{0, 1, 0});
}

#endif