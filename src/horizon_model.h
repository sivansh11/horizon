#ifndef HORIZON_MODEL_H
#define HORIZON_MODEL_H

#include "horizon_device.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace horizon
{

class HorizonModel
{
public:
    struct Vertex
    {
        glm::vec2 position;
        glm::vec3 color;

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };
    

    HorizonModel(HorizonDevice &device, const std::vector<Vertex> &vertices);
    ~HorizonModel();

    HorizonModel(const HorizonModel&) = delete;
    HorizonModel* operator=(const HorizonModel&) = delete;

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

private:
    void createVertexBuffers(const std::vector<Vertex> vertices);

private:
    HorizonDevice &horizonDevice;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t vertexCount; 
};

} // namespace horizon


#endif