#include "first_app.h"

#include "systems/simple_render_system.h"
#include "systems/keyboard_movement_controller.h"

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

// temporary helper function, creates a 1x1x1 cube centered at offset with an index buffer
std::unique_ptr<HorizonModel> createCubeModel(HorizonDevice& device, glm::vec3 offset) {
  HorizonModel::Builder modelBuilder{};
  modelBuilder.vertices = {
      // left face (white)
      {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
      {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

      // right face (yellow)
      {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .8f, .1f}},
      {{.5f, -.5f, .5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, -.5f}, {.8f, .8f, .1f}},

      // top face (orange, remember y axis points down)
      {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},
      {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}},

      // bottom face (red)
      {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
      {{-.5f, .5f, .5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, -.5f}, {.8f, .1f, .1f}},

      // nose face (blue)
      {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
      {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},

      // tail face (green)
      {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
      {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
  };
  for (auto& v : modelBuilder.vertices) {
    v.position += offset;
  }

  modelBuilder.indices = {0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
                          12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21};

  return std::make_unique<HorizonModel>(device, modelBuilder);
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
    camera.setViewTarget(glm::vec3{-1, -2, 2}, glm::vec3{0, 0, 2.5});
    HorizonClock clock;

    auto viewerObject = HorizonGameObject::createGameObject();
    KeyboardMovementController cameraController{};

    while (!horizonWindow.shouldClose())
    {
        glfwPollEvents();
        float frameTime = clock.frameTime<float>();

        cameraController.moveInPlaneXZ(horizonWindow.getGLFWwindow(), frameTime, viewerObject);
        camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        float aspect = horizonRenderer.getAspectRatio();
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
