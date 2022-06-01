#ifndef HSR_WINDOWS_H
#define HSR_WINDOWS_H

#include "horizon_core.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace horizon
{

class HorizonWindow
{
public:
    HorizonWindow(uint width, uint height, std::string name);
    ~HorizonWindow();

    HorizonWindow(const HorizonWindow&) = delete;
    HorizonWindow& operator=(const HorizonWindow&) = delete;

    bool shouldClose() { return glfwWindowShouldClose(window); }
    VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }

    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

    bool wasWindowResized() { return frameBufferResized; }
    void resetWindowResizedFlag() { frameBufferResized = false; }

    GLFWwindow* getGLFWwindow() const { return window; }

private:
    void initWindow();
    static void frameBufferResizeCallback(GLFWwindow *window, int width, int height);

private:
    std::string windowName;
    GLFWwindow *window;
    int width, height;
    bool frameBufferResized = false;

};
   
} // namespace horizon


#endif 