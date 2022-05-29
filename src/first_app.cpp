#include "first_app.h"

#include "glm/gtc/constants.hpp"

namespace horizon
{


struct SimplePushConstantData
{
    glm::mat2 transform{1.f};
    glm::vec2 offset;
    alignas(16) glm::vec3 color;
};

FirstApp::FirstApp()
{
    loadGameObjects();
    createPipelineLayout();
    createPipeline();
}
FirstApp::~FirstApp()
{
    vkDestroyPipelineLayout(horizonDevice.device(), pipelineLayout, nullptr);
}

void FirstApp::createPipelineLayout()
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

void FirstApp::createPipeline()
{
    ASSERT(pipelineLayout != nullptr, "cannot create pipeline before pipeline layout");
    PipelineConfigInfo pipelineConfig{};
    HorizonPipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass = horizonRenderer.getSwapChainRenderPass();
    pipelineConfig.pipelineLayout = pipelineLayout;
    horizonPipeline = std::make_unique<HorizonPipeline>(horizonDevice, "../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv", pipelineConfig);
}

void FirstApp::loadGameObjects()
{
    std::vector<HorizonModel::Vertex> vertices{
        {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}
    };
    auto horizonModel = std::make_shared<HorizonModel>(horizonDevice, vertices);

    auto triangle = HorizonGameObject::createGameObject();

    triangle.model = horizonModel;
    triangle.color = {.1f, .8f, .1f};
    triangle.transform2d.translation.x = .2f;
    triangle.transform2d.scale = {2.f, .5f};
    triangle.transform2d.rotation = .25f * glm::two_pi<float>();
    
    gameObjects.push_back(std::move(triangle));
}

void FirstApp::run()
{
    while (!horizonWindow.shouldClose())
    {
        glfwPollEvents();

        if (auto commandBuffer = horizonRenderer.beginFrame())
        {
            horizonRenderer.beginSwapchainRenderPass(commandBuffer);
            renderGameObjects(commandBuffer);
            horizonRenderer.endSwapchainRenderPass(commandBuffer);
            horizonRenderer.endFrame();
        }
    }

    vkDeviceWaitIdle(horizonDevice.device());
}


void FirstApp::renderGameObjects(VkCommandBuffer commandBuffer)
{
    horizonPipeline->bind(commandBuffer);
    for (auto& obj: gameObjects)
    {
        obj.transform2d.rotation = glm::mod(obj.transform2d.rotation + 0.01f, glm::two_pi<float>());

        SimplePushConstantData push{};
        push.offset = obj.transform2d.translation;
        push.color = obj.color;
        push.transform = obj.transform2d.mat2();

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);
        obj.model->bind(commandBuffer);
        obj.model->draw(commandBuffer);
    }
}

} // namespace horizon
