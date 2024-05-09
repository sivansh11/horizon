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

std::tuple<std::vector<triangle_t>, std::vector<bvh_t>> create_blas_from_model(const core::model_t& model) {
    
}

struct transform_t {
    glm::vec3 translation{ 0.f, 0.f, 0.f };
    glm::vec3 rotation{ 0.f, 0.f, 0.f };
    glm::vec3 scale{ 1.f, 1.f, 1.f };
    
    transform_t() = default;

    glm::mat4 mat4() const {
        return glm::translate(glm::mat4(1.f), translation)
            * glm::toMat4(glm::quat(rotation))
            * glm::scale(glm::mat4(1.f), scale);
    }
};

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
    
    float fov{90.0f};  // zoom var ?
    float camera_speed_multiplyer{1};

    float far{1000.f};
    float near{0.1f};

private:
    core::window_t& _window;

    glm::vec3 _position {0, 0, -5};
    
    glm::mat4 _projection{1.0f};
    glm::mat4 _view{1.0f};

    glm::vec3 _front{0, 0, 1};
    glm::vec3 _up{0, 1, 0};
    glm::vec3 _right{-1, 0, 0};

    glm::vec2 _initial_mouse{};

    float _yaw{90};
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