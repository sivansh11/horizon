#include "editor_camera.hpp"

#include <GLFW/glfw3.h>

void editor_camera_t::update_projection(float aspect_ratio) {
    static float s_aspect_ratio = 0;
    if (s_aspect_ratio != aspect_ratio) {
        _projection = glm::perspective(glm::radians(fov), aspect_ratio, near, far);
        s_aspect_ratio = aspect_ratio;
    }
}

void editor_camera_t::update(float ts) {
    auto [width, height] = _window->dimensions();
    update_projection(float(width) / float(height));

    double curX, curY;
    glfwGetCursorPos(_window->window(), &curX, &curY);

    float velocity = _mouse_speed * ts;

    if (glfwGetKey(_window->window(), GLFW_KEY_W)) 
        _position += _front * velocity;
    if (glfwGetKey(_window->window(), GLFW_KEY_S)) 
        _position -= _front * velocity;
    if (glfwGetKey(_window->window(), GLFW_KEY_D)) 
        _position += _right * velocity;
    if (glfwGetKey(_window->window(), GLFW_KEY_A)) 
        _position -= _right * velocity;
    if (glfwGetKey(_window->window(), GLFW_KEY_SPACE)) 
        _position += _up * velocity;
    if (glfwGetKey(_window->window(), GLFW_KEY_LEFT_SHIFT)) 
        _position -= _up * velocity;
    
    glm::vec2 mouse{curX, curY};
    glm::vec2 difference = mouse - _initial_mouse;
    _initial_mouse = mouse;

    if (glfwGetMouseButton(_window->window(), GLFW_MOUSE_BUTTON_1)) {
        
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
