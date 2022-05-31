#ifndef HORIZON_CAMERA_H
#define HORIZON_CAMERA_H

#include "horizon_core.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace horizon
{

class HorizonCamera
{
public:
    void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
    void setPerspectiveProjection(float fovy, float aspect, float near, float far);

    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = {0.f, -1.f, 0.f});
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = {0.f, -1.f, 0.f});
    void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

    const glm::mat4 &getProjection() const { return projectionMatrix; }
    const glm::mat4 &getView() const { return viewMatrix; }
    
private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};

};

} // namespace horizon


#endif