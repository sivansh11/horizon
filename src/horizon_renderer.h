#ifndef HORIZON_RENDERER_H
#define HORIZON_RENDERER_H

#include "horizon_core.h"

#include "horizon_window.h"
#include "horizon_device.h"
#include "horizon_model.h"
#include "horizon_swapchain.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace horizon
{
class HorizonRenderer
{
public:
    HorizonRenderer(HorizonWindow &window, HorizonDevice &device);
    ~HorizonRenderer();

    HorizonRenderer(const HorizonRenderer&) = delete;
    HorizonRenderer* operator=(const HorizonRenderer&) = delete;

    VkRenderPass getSwapChainRenderPass() const { return horizonSwapChain->getRenderPass(); }

    bool isFrameInProgress() const { return isFrameStarted; }
    VkCommandBuffer getCurrentCommandBuffer() const 
    {
        ASSERT(isFrameInProgress() == true, "cannot get command buffer while frame not in progress");
        return commandBuffers[currentImageIndex];
    }

    VkCommandBuffer beginFrame();
    void endFrame();

    void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapchainRenderPass(VkCommandBuffer commandBuffer);

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

private:
    HorizonWindow& horizonWindow;
    HorizonDevice& horizonDevice;
    std::unique_ptr<HorizonSwapChain> horizonSwapChain;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t currentImageIndex = 0;
    bool isFrameStarted = false;
};
} // namespace horizon

#endif