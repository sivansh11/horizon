#include "first_app.h"


namespace horizon
{

FirstApp::FirstApp()
{
    createPipelineLayout();
    createPipeline();
    createCommandBuffers();
}
FirstApp::~FirstApp()
{
    vkDestroyPipelineLayout(horizonDevice.device(), pipelineLayout, nullptr);
}

void FirstApp::createPipelineLayout()
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(horizonDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        runtime_assert(false, "failed to create pipeline layout");
    }
}

void FirstApp::createPipeline()
{
    auto pipelineConfig = HorizonPipeline::defaultPipelineConfigInfo(horizonSwapChain.width(), horizonSwapChain.height());
    pipelineConfig.renderPass = horizonSwapChain.getRenderPass();
    pipelineConfig.pipelineLayout = pipelineLayout;
    horizonPipeline = std::make_unique<HorizonPipeline>(horizonDevice, "../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv", pipelineConfig);
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
    commandBuffers.resize(horizonSwapChain.imageCount());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = horizonDevice.getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(horizonDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        runtime_assert(false, "failed to allocate commandbuffers");
    }

    for (int i = 0; i < commandBuffers.size(); i++)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
        {
            runtime_assert(false, "failed to record commandbuffer");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = horizonSwapChain.getRenderPass();
        renderPassInfo.framebuffer = horizonSwapChain.getFrameBuffer(i);

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = horizonSwapChain.getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        horizonPipeline->bind(commandBuffers[i]);
        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
        {
            runtime_assert(false, "failed to record command buffer");
        }
    }
}
void FirstApp::drawFrame() 
{
    uint32_t imageIndex;
    auto result = horizonSwapChain.acquireNextImage(&imageIndex);

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        runtime_assert(false, "failed to acquire swap chain image");
    }

    result = horizonSwapChain.submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);

    if (result != VK_SUCCESS)
    {
        runtime_assert(false, "failed to present swap chain image");
    }
}


} // namespace horizon
