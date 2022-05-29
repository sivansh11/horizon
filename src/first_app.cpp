#include "first_app.h"

#include "simple_render_system.h"

#include "glm/gtc/constants.hpp"

namespace horizon
{

FirstApp::FirstApp()
{
    loadGameObjects();
}
FirstApp::~FirstApp()
{

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
    SimpleRenderSystem simpleRenderSystem{horizonDevice, horizonRenderer.getSwapChainRenderPass()};

    while (!horizonWindow.shouldClose())
    {
        glfwPollEvents();

        if (auto commandBuffer = horizonRenderer.beginFrame())
        {
            horizonRenderer.beginSwapchainRenderPass(commandBuffer);
            simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects);
            horizonRenderer.endSwapchainRenderPass(commandBuffer);
            horizonRenderer.endFrame();
        }
    }

    vkDeviceWaitIdle(horizonDevice.device());
}

} // namespace horizon
