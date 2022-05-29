#include "horizon_renderer.h"

namespace horizon
{

HorizonRenderer::HorizonRenderer(HorizonWindow &window, HorizonDevice &device) :
    horizonWindow(window), horizonDevice(device)
{
    recreateSwapChain();
    createCommandBuffers();
}
HorizonRenderer::~HorizonRenderer()
{
    freeCommandBuffers();
}

void HorizonRenderer::recreateSwapChain()
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
        std::shared_ptr<HorizonSwapChain> oldSwapChain = std::move(horizonSwapChain);
        horizonSwapChain = std::make_unique<HorizonSwapChain>(horizonDevice, extent, oldSwapChain);
        if (horizonSwapChain->imageCount() != commandBuffers.size())
        {
            freeCommandBuffers();
            createCommandBuffers();
        }
    }
}

void HorizonRenderer::createCommandBuffers() 
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
void HorizonRenderer::freeCommandBuffers()
{
    vkFreeCommandBuffers(horizonDevice.device(), horizonDevice.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
}


VkCommandBuffer HorizonRenderer::beginFrame()
{
    ASSERT(!isFrameStarted, "cant call begin frame while already in progress");
    auto result = horizonSwapChain->acquireNextImage(&currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return nullptr;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        RUNTIME_ASSERT(false, "failed to acquire swap chain image");
    }
    isFrameStarted = true;

    auto commandBuffer = getCurrentCommandBuffer();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to record commandbuffer");
    }
    return commandBuffer;
}
void HorizonRenderer::endFrame()
{
    ASSERT(isFrameStarted, "cant call endFrame while frame not in progress");
    auto commandBuffer = getCurrentCommandBuffer();
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to record command buffer");
    }

    auto result = horizonSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || horizonWindow.wasWindowResized())
    {
        horizonWindow.resetWindowResizedFlag();
        recreateSwapChain();
    }

    if (result != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to present swap chain image");
    }
    isFrameStarted = false;
}

void HorizonRenderer::beginSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    ASSERT(isFrameStarted, "cant call beginSwapchainRenderPass while frame not in progress");
    ASSERT(commandBuffer == getCurrentCommandBuffer(), "cant begin render pass on command buffer from a different frmae");

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = horizonSwapChain->getRenderPass();
    renderPassInfo.framebuffer = horizonSwapChain->getFrameBuffer(currentImageIndex);

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = horizonSwapChain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(horizonSwapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(horizonSwapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, horizonSwapChain->getSwapChainExtent()};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

}
void HorizonRenderer::endSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    ASSERT(isFrameStarted, "cant call endSwapchainRenderPass while frame not in progress");
    ASSERT(commandBuffer == getCurrentCommandBuffer(), "cant end render pass on command buffer from a different frmae");

    vkCmdEndRenderPass(commandBuffer);
}

} // namespace horizon
