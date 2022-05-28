#ifndef FIRST_APP_H
#define FIRST_APP_H

#include "horizon_core.h"

#include "horizon_window.h"
#include "horizon_pipeline.h"
#include "horizon_device.h"
#include "horizon_model.h"
#include "horizon_swapchain.h"
#include "horizon_game_object.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace horizon
{
class FirstApp
{
public:
    static constexpr uint WIDTH = 800;
    static constexpr uint HEIGHT = 600;
    FirstApp();
    ~FirstApp();

    FirstApp(const FirstApp&) = delete;
    FirstApp* operator=(const FirstApp&) = delete;

    void run();

private:
    void loadGameObjects();
    void renderGameObjects(VkCommandBuffer commandBuffer);
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();
    void freeCommandBuffers();
    void drawFrame();
    void recreateSwapChain();
    void recordCommandBuffer(int imageIndex);

private:
    HorizonWindow horizonWindow{WIDTH, HEIGHT, "First app!"};
    HorizonDevice horizonDevice{horizonWindow};
    std::unique_ptr<HorizonSwapChain> horizonSwapChain;
    std::unique_ptr<HorizonPipeline> horizonPipeline;
    VkPipelineLayout pipelineLayout;
    std::vector<VkCommandBuffer> commandBuffers;
    // std::unique_ptr<HorizonModel> horizonModel;
    std::vector<HorizonGameObject> gameObjects;
};
} // namespace horizon

#endif