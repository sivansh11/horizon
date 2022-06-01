#include "horizon_model.h"

#include "horizon_utils.h"

#include <tinyobjloader.h>

namespace std
{

template <>
struct hash<horizon::HorizonModel::Vertex>
{
    size_t operator()(horizon::HorizonModel::Vertex const &vertex) const
    {
        size_t seed = 0;
        horizon::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
        return seed;
    }
};

} // namespace std


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

std::unique_ptr<HorizonModel> HorizonModel::createModelFromFile(HorizonDevice &device, const char *filePath)
{
    Builder builder{};
    builder.loadModel(filePath);
    return std::make_unique<HorizonModel>(device, builder);
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
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
    attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
    attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
    return attributeDescriptions;    
}

void HorizonModel::Builder::loadModel(const char *filePath)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath))
    {
        RUNTIME_ASSERT(false, (warn + err).c_str());
    }

    vertices.clear();
    indices.clear();

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape: shapes)
    {
        for (const auto& index: shape.mesh.indices)
        {
            Vertex vertex{};

            if (index.vertex_index >= 0)
            {
                vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };
                
                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2]
                };
            }

            if (index.normal_index >= 0)
            {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            if (index.texcoord_index >= 0)
            {
                vertex.uv = {
                    attrib.texcoords[2 * index.vertex_index + 0],
                    attrib.texcoords[2 * index.vertex_index + 1]
                };
            }

            // vertices.push_back(vertex);
            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);

        }
    }
}


} // namespace horizon
