#include "horizon_window.h"

namespace horizon
{

HorizonWindow::HorizonWindow(uint width, uint height, std::string name) :
    width(width),
    height(height),
    windowName(name)
{
    initWindow();
}
HorizonWindow::~HorizonWindow()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}   

void HorizonWindow::initWindow()
{
    runtime_assert(glfwInit() == GLFW_TRUE, "GLFW failed to initialize!");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, windowName.c_str(), NULL, NULL);
}

void HorizonWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface)
{
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
    {
        runtime_assert(false, "failed to create window surface!");
    }
}

} // namespace horizon
