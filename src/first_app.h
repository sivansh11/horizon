#ifndef FIRST_APP_H
#define FIRST_APP_H

#include "horizon_core.h"

#include "horizon_window.h"
#include "horizon_pipeline.h"
#include "horizon_device.h"
#include "horizon_swapchain.h"

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
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();
    void drawFrame();

private:
    HorizonWindow horizonWindow{WIDTH, HEIGHT, "First app!"};
    HorizonDevice horizonDevice{horizonWindow};
    HorizonSwapChain horizonSwapChain{horizonDevice, horizonWindow.getExtent()};
    std::unique_ptr<HorizonPipeline> horizonPipeline;
    VkPipelineLayout pipelineLayout;
    std::vector<VkCommandBuffer> commandBuffers;
    // HorizonPipeline horizonPipeline{horizonDevice, "../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv", HorizonPipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
};
} // namespace horizon

#endif