#ifndef HORIZON_MODEL_H
#define HORIZON_MODEL_H

#include "horizon_device.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

namespace horizon
{

class HorizonModel
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 color;
        glm::vec3 normal{};
        glm::vec2 uv{};

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const Vertex &other) const 
        {
            return position == other.position &&
                   color == other.color &&
                   normal == other.normal &&
                   uv == other.uv;
        }
    };

    struct Builder
    {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};

        void loadModel(const char *filePath);
    };

    HorizonModel(HorizonDevice &device, const HorizonModel::Builder &builder);
    ~HorizonModel();

    HorizonModel(const HorizonModel&) = delete;
    HorizonModel* operator=(const HorizonModel&) = delete;

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

    static std::unique_ptr<HorizonModel> createModelFromFile(HorizonDevice &device, const char *filePath);

private:
    void createVertexBuffers(const std::vector<Vertex> &vertices);
    void createIndexBuffers(const std::vector<uint32_t> &indices);

private:
    HorizonDevice &horizonDevice;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t vertexCount; 

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount; 

    bool hasIndexBuffer = false;
};

} // namespace horizon


#endif