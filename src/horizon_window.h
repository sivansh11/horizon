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

    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

private:
    void initWindow();

private:
    std::string windowName;
    GLFWwindow *window;
    uint width, height;

};
   
} // namespace horizon


#endif 