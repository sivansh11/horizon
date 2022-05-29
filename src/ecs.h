#ifndef HORIZON_ECS_H
#define HORIZON_ECS_H

#include "horizon_core.h"

namespace horizon
{
namespace ecs
{

#define MAX_COMPONENTS 32
#define MAX_ENTITIES 100000

using EntityID = unsigned long long;
using ComponentID = uint32_t;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

static ComponentID s_ComponentCounter = 0;

template <typename T>
ComponentID getComponentID() 
{
    static ComponentID s_ComponentID = s_ComponentCounter++;
    return s_ComponentID;
}

class BaseComponentPool
{
public:
    virtual void* get(size_t index) = 0;
    virtual void* construct(size_t index) = 0;
private:

};
template <typename T>
class ComponentPool : public BaseComponentPool
{
public:
    ComponentPool()
    {
        m_componentSize = sizeof(T);
        p_data = new char[m_componentSize * MAX_ENTITIES];
    }
    ~ComponentPool()
    {
        delete[] p_data;
    }

    void* get(size_t index) override
    {
        return p_data + index * m_componentSize;
    }
    void* construct(size_t index) override
    {
        return new (get(index)) T();
    }
private:
    char* p_data{nullptr};
    size_t m_componentSize{0};
};

class Scene
{
    struct EntityDescription
    {
        EntityID id;
        ComponentMask mask;
        bool isValid;
    };
public:
    Scene()
    {
        for (int i = 0; i < MAX_ENTITIES; i++)
        {
            availableIDs.push(static_cast<EntityID>(i));
        }
    }
    ~Scene()
    {
        for (auto& componentPool: componentPools)
        {
            delete componentPool;
        }
    }
    const EntityID newEntity()
    {
        // entities.push_back({entities.size(), ComponentMask()});
        EntityID entityID = availableIDs.front();
        availableIDs.pop();
        if (entities.size() <= entityID)
        {
            entities.push_back({entityID, ComponentMask(), true});
        }
        ASSERT(entities[entityID].isValid == true, "this entity should not be in the available queue");
        return entityID;
    }
    void deleteEntity(const EntityID entityID)
    {
        ASSERT(entities[entityID].isValid == true, "cannot delete already deleted entity");
        entities[entityID].isValid = false;
        entities[entityID].mask.reset();
        availableIDs.push(entityID);
    }
    template <typename T>
    T& assign(EntityID entityID)
    {
        ASSERT(entities[entityID].isValid == true, "entity does not exist");
        ComponentID componentID = getComponentID<T>();
        if (componentPools.size() <= componentID)
        {
            componentPools.resize(componentID + 1, nullptr);
        }
        if (componentPools[componentID] == nullptr)
        {
            componentPools[componentID] = new ComponentPool<T>();
        }
        T* p_component = (T*)(componentPools[componentID]->construct(entityID));
        entities[entityID].mask.set(componentID);
        return *p_component;
    }
    template <typename T>
    void remove(EntityID entityID)
    {
        ComponentID componentID = getComponentID<T>();
        ASSERT(isValid(entityID) == true, "entity does not exist");
        ASSERT(entities[entityID].mask.test(componentID) == true, "cannot remove component from entity which doesnt contain it in the first place");
        entities[entityID].mask.reset(componentID);
    }
    template <typename T>
    bool has(EntityID entityID)
    {
        ASSERT(isValid(entityID) == true, "entity does not exist");
        return entities[entityID].mask.test(getComponentID<T>());
    }
    template <typename T>
    T& get(EntityID entityID)
    {
        ASSERT(entities[entityID].isValid == true, "entity does not exist");
        ASSERT(has<T>(entityID) == true, "entity does not contain requested component");
        ComponentID componentID = getComponentID<T>();
        return *((T*)(componentPools[componentID]->get(entityID)));
    }
    bool isValid(EntityID entityID)
    {
        ASSERT(entities.size() > entityID, "entity doesnt exist");
        return entities[entityID].isValid;
    }
private:
    std::vector<EntityDescription> entities;
    std::queue<EntityID> availableIDs;
    std::vector<BaseComponentPool*> componentPools;

};

template <typename... ComponentTypes>
struct SceneView
{
    SceneView(Scene &scene) : scene(scene)
    {
        if (sizeof...(ComponentTypes) == 0)
        {
            all = true;
        }
        else
        {
            int componentIDs[] = {getComponentID<ComponentTypes>()...};
            for (int i = 0; i < sizeof...(ComponentTypes); i++)
            {
                componentMask.set(componentIDs[i]);
            }
        }
    }
    struct Iterator
    {
        Iterator(Scene &scene, EntityID index, ComponentMask mask, bool all) :
            scene(scene), index(index), mask(mask), all(all)
        {

        }

        bool isValidIndex()
        {
            return scene.isValid(scene.entities[index].id) && (all || mask == (mask & scene.entities[index].mask));
        }
        EntityID operator*() const
        {
            return scene.entities[index].id;
        }
        bool operator==(const Iterator &other) const
        {
            return index == other.index || index == scene.entities.size();
        }
        bool operator!=(const Iterator &other) const
        {
            return index != other.index && index != scene.entities.size();
        }
        Iterator& operator++()
        {
            do 
            {
                index++;
            } while(index < scene.entities.size() && !isValidIndex());
            return *this;
        }
        EntityID index;
        Scene &scene;
        ComponentMask mask;
        bool all = false;
    };

    const Iterator begin() const
    {
        EntityID firstIndex = 0;
        while (firstIndex < scene.entities.size() && (componentMask != (componentMask & scene.entities[firstIndex].mask) || scene.isValid(scene.entities[firstIndex].id)))
        {
            firstIndex++;
        } 
        
        return Iterator(scene, firstIndex, componentMask, all);
    }
    
    const Iterator end() const 
    {
        return Iterator(scene, EntityID{scene.entities.size()}, componentMask, all);
    }
    Scene &scene;
    bool all = false;
    ComponentMask componentMask;
};

} // namespace ecs
} // namespace horizon


#endif