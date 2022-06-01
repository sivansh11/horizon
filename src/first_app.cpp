#include "first_app.h"

#include "simple_render_system.h"

#include "horizon_camera.h"

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

// temporary helper function, creates a 1x1x1 cube centered at offset
std::unique_ptr<HorizonModel> createCubeModel(HorizonDevice& device, glm::vec3 offset) {
  std::vector<HorizonModel::Vertex> vertices{

      // left face (white)
      {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
      {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
      {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},

      // right face (yellow)
      {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .8f, .1f}},
      {{.5f, -.5f, .5f}, {.8f, .8f, .1f}},
      {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, -.5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .8f, .1f}},

      // top face (orange, remember y axis points down)
      {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},
      {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
      {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},

      // bottom face (red)
      {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
      {{-.5f, .5f, .5f}, {.8f, .1f, .1f}},
      {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, -.5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .1f, .1f}},

      // nose face (blue)
      {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
      {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
      {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},

      // tail face (green)
      {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
      {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
      {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},

  };
  for (auto& v : vertices) {
    v.position += offset;
  }
  return std::make_unique<HorizonModel>(device, vertices);
}

void FirstApp::loadGameObjects()
{
    std::shared_ptr<HorizonModel> horizonModel = createCubeModel(horizonDevice, {.0f, .0f, .0f});

    auto cube = HorizonGameObject::createGameObject();
    cube.model = horizonModel;
    cube.transform.translation = {0.f, 0.f, 2.5f};
    cube.transform.scale = {.5, .5, .5};
    gameObjects.push_back(std::move(cube));
    
}

void FirstApp::run()
{
    SimpleRenderSystem simpleRenderSystem{horizonDevice, horizonRenderer.getSwapChainRenderPass()};
    HorizonCamera camera{};
    // camera.setViewDirection(glm::vec3{0.f}, glm::vec3{.5, 0, 1});
    camera.setViewTarget(glm::vec3{-1, -2, 2}, glm::vec3{0, 0, 2.5});
    HorizonClock clock;

    while (!horizonWindow.shouldClose())
    {
        glfwPollEvents();
        std::cout << 1.0f / clock.frameTime<float>() << '\n';

        float aspect = horizonRenderer.getAspectRatio();
        // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
        camera.setPerspectiveProjection(glm::radians(60.0f), aspect, .1, 100);
        if (auto commandBuffer = horizonRenderer.beginFrame())
        {
            horizonRenderer.beginSwapchainRenderPass(commandBuffer);
            simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);
            horizonRenderer.endSwapchainRenderPass(commandBuffer);
            horizonRenderer.endFrame();
        }
    }

    vkDeviceWaitIdle(horizonDevice.device());
}

} // namespace horizon
