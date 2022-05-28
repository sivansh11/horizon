#ifndef HORIZON_GAME_OBJECT_H
#define HORIZON_GAME_OBJECT_H

#include "horizon_core.h"

#include "horizon_model.h"

namespace horizon
{

struct Transform2DComponent
{
    glm::vec2 translation{};
    glm::vec2 scale{1.f, 1.f};
    float rotation;

    glm::mat2 mat2() 
    { 
        const float s = glm::sin(rotation);
        const float c = glm::cos(rotation);

        glm::mat2 rotMat{{c, s}, {-s, c}};

        glm::mat2 scaleMat{{scale.x, 0.f}, {0.f, scale.y}};
        return rotMat * scaleMat;
    }
};

class HorizonGameObject
{
public:
    using id_t = unsigned int;

    HorizonGameObject(const HorizonGameObject&) = delete;
    HorizonGameObject& operator=(const HorizonGameObject*) = delete;
    HorizonGameObject(HorizonGameObject&&) = default;
    HorizonGameObject& operator=(HorizonGameObject&&) = default;

    static HorizonGameObject createGameObject()
    {
        static id_t current_id = 0;
        return HorizonGameObject(current_id++);
    }

    const id_t getID() const { return id; }

    std::shared_ptr<HorizonModel> model{};
    glm::vec3 color;   
    Transform2DComponent transform2d; 

private:
    HorizonGameObject(id_t objID) : id(objID) {}

private:
    id_t id;

};

} // namespace horizon


#endif