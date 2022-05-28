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
    recreateSwapChain();
    createCommandBuffers();
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
    ASSERT(horizonSwapChain != nullptr, "cannot create pipeline before swapchain");
    ASSERT(pipelineLayout != nullptr, "cannot create pipeline before pipeline layout");
    PipelineConfigInfo pipelineConfig{};
    HorizonPipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass = horizonSwapChain->getRenderPass();
    pipelineConfig.pipelineLayout = pipelineLayout;
    horizonPipeline = std::make_unique<HorizonPipeline>(horizonDevice, "../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv", pipelineConfig);
}

void FirstApp::recreateSwapChain()
{
    auto extent =  horizonWindow.getExtent();
    while (extent.width == 0 || extent.height == 0)
    {
        extent = horizonWindow.getExtent();
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(horizonDevice.device());
    if (horizonSwapChain == nullptr)
    {
        horizonSwapChain = std::make_unique<HorizonSwapChain>(horizonDevice, extent);
    }
    else
    {
        horizonSwapChain = std::make_unique<HorizonSwapChain>(horizonDevice, extent, std::move(horizonSwapChain));
        if (horizonSwapChain->imageCount() != commandBuffers.size())
        {
            freeCommandBuffers();
            createCommandBuffers();
        }
    }
    createPipeline();    
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
        drawFrame();
    }

    vkDeviceWaitIdle(horizonDevice.device());
}

void FirstApp::createCommandBuffers() 
{
    commandBuffers.resize(horizonSwapChain->imageCount());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = horizonDevice.getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(horizonDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to allocate commandbuffers");
    }
}
void FirstApp::freeCommandBuffers()
{
    vkFreeCommandBuffers(horizonDevice.device(), horizonDevice.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
}


void FirstApp::recordCommandBuffer(int imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to record commandbuffer");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = horizonSwapChain->getRenderPass();
    renderPassInfo.framebuffer = horizonSwapChain->getFrameBuffer(imageIndex);

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = horizonSwapChain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(horizonSwapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(horizonSwapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, horizonSwapChain->getSwapChainExtent()};
    vkCmdSetViewport(commandBuffers[imageIndex], 0, 1, &viewport);
    vkCmdSetScissor(commandBuffers[imageIndex], 0, 1, &scissor);

    renderGameObjects(commandBuffers[imageIndex]);

    vkCmdEndRenderPass(commandBuffers[imageIndex]);
    if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to record command buffer");
    }
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

void FirstApp::drawFrame() 
{
    uint32_t imageIndex;
    auto result = horizonSwapChain->acquireNextImage(&imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        RUNTIME_ASSERT(false, "failed to acquire swap chain image");
    }

    recordCommandBuffer(imageIndex);

    result = horizonSwapChain->submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || horizonWindow.wasWindowResized())
    {
        horizonWindow.resetWindowResizedFlag();
        recreateSwapChain();
        return;
    }

    if (result != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to present swap chain image");
    }
}


} // namespace horizon
