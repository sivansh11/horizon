#ifndef CORE_COMPONENTS_HPP
#define CORE_COMPONENTS_HPP

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

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

} // namespace core

#endif