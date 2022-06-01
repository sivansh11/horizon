#ifndef KEYBOARD_MOVEMENT_CONTROLLER_H
#define KEYBOARD_MOVEMENT_CONTROLLER_H

#include "horizon_game_object.h"
#include "horizon_window.h"

namespace horizon
{

class KeyboardMovementController
{
public:
    struct KeyMappings 
    {
        int moveLeft = GLFW_KEY_A;
        int moveRight = GLFW_KEY_D;
        int moveForward = GLFW_KEY_W;
        int moveBackward = GLFW_KEY_S;
        int moveUp = GLFW_KEY_SPACE;
        int moveDown = GLFW_KEY_LEFT_SHIFT;
        int lookLeft = GLFW_KEY_LEFT;
        int lookRight = GLFW_KEY_RIGHT;
        int lookUp = GLFW_KEY_UP;
        int lookDown = GLFW_KEY_DOWN;
    };

    void moveInPlaneXZ(GLFWwindow* window, float dt, HorizonGameObject &gameObject);

    KeyMappings keys;
    float moveSpeed{3.f};
    float lookSpeed{1.5f};


};

} // namespace horizon


#endif