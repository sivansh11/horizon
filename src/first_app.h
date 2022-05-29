#ifndef FIRST_APP_H
#define FIRST_APP_H

#include "horizon_core.h"

#include "horizon_window.h"
#include "horizon_device.h"
#include "horizon_model.h"
#include "horizon_game_object.h"
#include "horizon_renderer.h"

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

private:
    HorizonWindow horizonWindow{WIDTH, HEIGHT, "First app!"};
    HorizonDevice horizonDevice{horizonWindow};
    HorizonRenderer horizonRenderer{horizonWindow, horizonDevice};
    VkPipelineLayout pipelineLayout;
    std::vector<HorizonGameObject> gameObjects;
};
} // namespace horizon

#endif