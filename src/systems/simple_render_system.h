#ifndef SIMPLE_RENDER_SYSTEM_H
#define SIMPLE_RENDER_SYSTEM_H

#include "horizon_core.h"

#include "horizon_pipeline.h"
#include "horizon_device.h"
#include "horizon_model.h"
#include "horizon_game_object.h"
#include "horizon_camera.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace horizon
{
class SimpleRenderSystem
{
public:
    SimpleRenderSystem(HorizonDevice &device, VkRenderPass renderPass);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem&) = delete;
    SimpleRenderSystem* operator=(const SimpleRenderSystem&) = delete;

    void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<HorizonGameObject> &gameObjects, const HorizonCamera &camera);

private:
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);

private:
    HorizonDevice &horizonDevice;
    std::unique_ptr<HorizonPipeline> horizonPipeline;
    VkPipelineLayout pipelineLayout;
};
} // namespace horizon

#endif