#ifndef CORE_COMPONENTS_HPP
#define CORE_COMPONENTS_HPP

#include "math.hpp"

namespace core {

struct camera_t {
    void update();

    const glm::mat4& projection() const;
    const glm::mat4& inv_projection() const;
    const glm::mat4& view() const;
    const glm::mat4& inv_view() const;

    glm::vec3 position() const;

    glm::mat4 _projection{ 1.0f };
    glm::mat4 _inv_projection{ 1.0f };
    glm::mat4 _view{ 1.0f };
    glm::mat4 _inv_view{ 1.0f };
};

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

} // namespace core

#endif