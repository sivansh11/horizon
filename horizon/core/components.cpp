#include "components.hpp"

namespace core {

void camera_t::update() {
    _inv_projection = glm::inverse(_projection);
    _inv_view = glm::inverse(_view);
}

const glm::mat4& camera_t::projection() const {
    return _projection;
}

const glm::mat4& camera_t::inv_projection() const {
    return _inv_projection;
}

const glm::mat4& camera_t::view() const {
    return _view;
}

const glm::mat4& camera_t::inv_view() const {
    return _inv_view;
}

glm::vec3 camera_t::position() const {
    return glm::vec3{ _inv_view[3][0], _inv_view[3][1], _inv_view[3][2] };
}

} // namespace core
