#include "horizon_model.h"

#include "tinyobjloader.h"

namespace horizon
{

// HorizonModel::HorizonModel(HorizonDevice &device, const std::vector<Vertex> &vertices) :
//     horizonDevice(device)
// {
//     createVertexBuffers(vertices);
// }

HorizonModel::HorizonModel(HorizonDevice &device, const HorizonModel::Builder &builder) :
    horizonDevice(device)
{
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
}


HorizonModel::~HorizonModel()
{
    vkDestroyBuffer(horizonDevice.device(), vertexBuffer, nullptr);
    vkFreeMemory(horizonDevice.device(), vertexBufferMemory, nullptr);

    if (hasIndexBuffer)
    {
        vkDestroyBuffer(horizonDevice.device(), indexBuffer, nullptr);
        vkFreeMemory(horizonDevice.device(), indexBufferMemory, nullptr);
    }
}

// --------------------------------------------------------------------------------------------
// -------------------------------DYNAMIC BUFFERS----------------------------------------------
// --------------------------------------------------------------------------------------------
// void HorizonModel::createVertexBuffers(const std::vector<Vertex> &vertices)
// {
//     RUNTIME_ASSERT(vertices.size() >= 3, "model has too little vertices");
//     vertexCount = static_cast<uint32_t>(vertices.size());

//     VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

//     horizonDevice.createBuffer(
//         bufferSize,
//         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         vertexBuffer,
//         vertexBufferMemory
//     );

//     void *data;
//     vkMapMemory(horizonDevice.device(), vertexBufferMemory, 0, bufferSize, 0, &data);
//     memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
//     vkUnmapMemory(horizonDevice.device(), vertexBufferMemory);
// }

// void HorizonModel::createIndexBuffers(const std::vector<uint32_t> &indices)
// {
//     indexCount = static_cast<uint32_t>(indices.size());
//     hasIndexBuffer = indexCount > 0;
//     if (!hasIndexBuffer) return;

//     VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

//     horizonDevice.createBuffer(
//         bufferSize,
//         VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         indexBuffer,
//         indexBufferMemory
//     );

//     void *data;
//     vkMapMemory(horizonDevice.device(), indexBufferMemory, 0, bufferSize, 0, &data);
//     memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
//     vkUnmapMemory(horizonDevice.device(), indexBufferMemory);
// }

// --------------------------------------------------------------------------------------------
// --------------------------------STATIC BUFFERS----------------------------------------------
// --------------------------------------------------------------------------------------------
void HorizonModel::createVertexBuffers(const std::vector<Vertex> &vertices)
{
    RUNTIME_ASSERT(vertices.size() >= 3, "model has too little vertices");
    vertexCount = static_cast<uint32_t>(vertices.size());

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    horizonDevice.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void *data;
    vkMapMemory(horizonDevice.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(horizonDevice.device(), stagingBufferMemory);

    horizonDevice.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer,
        vertexBufferMemory
    );

    horizonDevice.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(horizonDevice.device(), stagingBuffer, nullptr);
    vkFreeMemory(horizonDevice.device(), stagingBufferMemory, nullptr);
}

void HorizonModel::createIndexBuffers(const std::vector<uint32_t> &indices)
{
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;
    if (!hasIndexBuffer) return;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    horizonDevice.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void *data;
    vkMapMemory(horizonDevice.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(horizonDevice.device(), stagingBufferMemory);

    horizonDevice.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer,
        indexBufferMemory
    );

    horizonDevice.copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(horizonDevice.device(), stagingBuffer, nullptr);
    vkFreeMemory(horizonDevice.device(), stagingBufferMemory, nullptr);

}

void HorizonModel::draw(VkCommandBuffer commandBuffer)
{
    if (hasIndexBuffer)
    {
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
}

void HorizonModel::bind(VkCommandBuffer commandBuffer)
{
    VkBuffer buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer)
    {
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

std::vector<VkVertexInputBindingDescription> HorizonModel::Vertex::getBindingDescriptions()
{
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> HorizonModel::Vertex::getAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    return attributeDescriptions;    
}


} // namespace horizon
