#include "first_app.h"


namespace horizon
{

FirstApp::FirstApp()
{

}
FirstApp::~FirstApp()
{

}
void FirstApp::run()
{
    while (!horizonWindow.shouldClose())
    {
        glfwPollEvents();
        
    }
    
    
}

} // namespace horizon
