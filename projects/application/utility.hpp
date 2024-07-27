#ifndef UTILITY_HPP
#define UTILITY_HPP

#include "core/window.hpp"
#include "core/components.hpp"
#include "core/math.hpp"

#include "gfx/bindless_manager.hpp"

#include <GLFW/glfw3.h>

#include <optional>

class camera_controller_t {
public:
    camera_controller_t(core::window_t& window) : _window(window) {}

    void update(float ts, core::camera_t& camera) {
        static float s_aspect_ratio = 0;
        auto [width, height] = _window.dimensions();
        float aspect_ratio = float(width) / float(height);
        if (s_aspect_ratio != aspect_ratio) {
            camera._projection = glm::perspective(glm::radians(fov), aspect_ratio, near, far);
            s_aspect_ratio = aspect_ratio;
        }
        
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

        camera._view = glm::lookAt(_position, _position + _front, glm::vec3{0, 1, 0});

        camera.update();
    }

public:
    
    float fov{90.0f};  // zoom var ?
    float camera_speed_multiplyer{1};

    float far{1000.f};
    float near{0.1f};

private:
    core::window_t& _window;

    glm::vec3 _position {0, 0, -5};

    glm::vec3 _front{0, 0, 1};
    glm::vec3 _up{0, 1, 0};
    glm::vec3 _right{-1, 0, 0};

    glm::vec2 _initial_mouse{};

    float _yaw{90};
    float _pitch{0};
    float _mouse_speed{0.005f};
    float _mouse_sensitivity{100.f};
};

#endif