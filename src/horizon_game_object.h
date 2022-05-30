#ifndef HORIZON_GAME_OBJECT_H
#define HORIZON_GAME_OBJECT_H

#include "horizon_core.h"

#include "horizon_model.h"

#include "ecs.h"

namespace horizon
{

struct TransformComponent
{
    glm::vec3 translation{};
    glm::vec3 scale{1.f, 1.f, 1.f};
    glm::vec3 rotation;

    glm::mat4 mat4()
    {
        auto transform = glm::translate(glm::mat4{1.f}, translation);
    
        transform = glm::rotate(transform, rotation.y, {0.f, 1.f, 0.f});
        transform = glm::rotate(transform, rotation.x, {1.f, 0.f, 0.f});
        transform = glm::rotate(transform, rotation.z, {0.f, 0.f, 1.f});

        transform = glm::scale(transform, scale);

        return transform;
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
    TransformComponent transform; 

private:
    HorizonGameObject(id_t objID) : id(objID) {}

private:
    id_t id;

};

} // namespace horizon


#endif