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
    RUNTIME_ASSERT(glfwInit() == GLFW_TRUE, "GLFW failed to initialize!");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, windowName.c_str(), NULL, NULL);
    RUNTIME_ASSERT(window != nullptr, "window creation failed!");
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);
}

void HorizonWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface)
{
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
    {
        RUNTIME_ASSERT(false, "failed to create window surface!");
    }
}
void HorizonWindow::frameBufferResizeCallback(GLFWwindow *window, int width, int height)
{
    horizon::HorizonWindow *horizonWindow = reinterpret_cast<HorizonWindow *>(glfwGetWindowUserPointer(window));
    horizonWindow->frameBufferResized = true;
    horizonWindow->width = width;
    horizonWindow->height = height;
}


} // namespace horizon
