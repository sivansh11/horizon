#include "simple_render_system.h"

#include "glm/gtc/constants.hpp"

namespace horizon
{


struct SimplePushConstantData
{
    glm::mat4 transform{1.f};
    glm::mat4 normalMatrix{1.f};
};

SimpleRenderSystem::SimpleRenderSystem(HorizonDevice &device, VkRenderPass renderPass) :
    horizonDevice(device)
{
    createPipelineLayout();
    createPipeline(renderPass);
}
SimpleRenderSystem::~SimpleRenderSystem()
{
    vkDestroyPipelineLayout(horizonDevice.device(), pipelineLayout, nullptr);
}

void SimpleRenderSystem::createPipelineLayout()
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(horizonDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to create pipeline layout");
    }
}

void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
{
    ASSERT(pipelineLayout != nullptr, "cannot create pipeline before pipeline layout");
    PipelineConfigInfo pipelineConfig{};
    HorizonPipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    horizonPipeline = std::make_unique<HorizonPipeline>(horizonDevice, "../assets/shaders/simple_shader.vert.spv", "../assets/shaders/simple_shader.frag.spv", pipelineConfig);
}


void SimpleRenderSystem::renderGameObjects(VkCommandBuffer commandBuffer, std::vector<HorizonGameObject> &gameObjects, const HorizonCamera &camera)
{
    auto projectionView = camera.getProjection() * camera.getView();

    horizonPipeline->bind(commandBuffer);
    for (auto& obj: gameObjects)
    {
        SimplePushConstantData push{};
        auto modelMatrix = obj.transform.mat4();
        push.transform = projectionView * modelMatrix;
        push.modelMatrix = modelMatrix;

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);
        obj.model->bind(commandBuffer);
        obj.model->draw(commandBuffer);
    }
}

} // namespace horizon
