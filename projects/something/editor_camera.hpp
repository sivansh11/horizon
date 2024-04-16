#ifndef EDITOR_CAMERA_HPP
#define EDITOR_CAMERA_HPP

#include "core/window.hpp"
#include "core/core.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <memory>
#include <iostream>

class editor_camera_t {
public:
    editor_camera_t(core::ref<core::window_t> window) : _window(window) {}
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
    core::ref<core::window_t> _window;

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

#endif