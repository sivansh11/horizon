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

void FirstApp::loadGameObjects()
{
    std::shared_ptr<HorizonModel> horizonModel = HorizonModel::createModelFromFile(horizonDevice, "../assets/models/flat_vase.obj");
    auto cube = HorizonGameObject::createGameObject();
    cube.model = horizonModel;
    cube.transform.translation = {0.f, 0.f, 2.5f};
    cube.transform.scale = glm::vec3{3.f, 1.5f, 3.f};
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
