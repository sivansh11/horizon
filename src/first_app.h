#ifndef FIRST_APP_H
#define FIRST_APP_H

#include "horizon_core.h"

#include "horizon_window.h"
#include "horizon_pipeline.h"
#include "horizon_device.h"

namespace horizon
{
class FirstApp
{
    static constexpr uint WIDTH = 800;
    static constexpr uint HEIGHT = 600;
public:
    FirstApp();
    ~FirstApp();

    void run();
    
private:
    HorizonWindow horizonWindow{WIDTH, HEIGHT, "first app"};
    HorizonDevice horizonDevice{horizonWindow};
    HorizonPipeline horizonPipeline{horizonDevice, "../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv", HorizonPipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
};
} // namespace horizon

#endif