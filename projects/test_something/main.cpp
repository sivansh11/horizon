#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#include "display.hpp"
#include "bvh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace core;

class editor_camera_t {
public:
    editor_camera_t(core::window_t& window) : _window(window) {}
    void update_projection(float aspect_ratio);
    void update(float ts);
    
    const glm::mat4& projection() const { return _projection; }
    const glm::mat4& view() const { return _view; }
    // creates a new mat4
    glm::mat4 inverse_projection() const { return glm::inverse(_projection); }
    // creates a new mat4
    glm::mat4 inverse_view() const { return glm::inverse(_view); }
    glm::vec3 front() const { return _front; }
    glm::vec3 right() const { return _right; }
    glm::vec3 up() const { return _up; }
    glm::vec3 position() const { return _position; }

public:
    
    float fov{45.0f};  // zoom var ?
    float camera_speed_multiplyer{1};

    float far{1000.f};
    float near{0.1f};

private:
    core::window_t& _window;

    glm::vec3 _position {0, 0, -5};
    
    glm::mat4 _projection{1.0f};
    glm::mat4 _view{1.0f};

    glm::vec3 _front{0, 0, 1};
    glm::vec3 _up{0, 1, 0};
    glm::vec3 _right{-1, 0, 0};

    glm::vec2 _initial_mouse{};

    float _yaw{90};
    float _pitch{0};
    float _mouse_speed{0.005f};
    float _mouse_sensitivity{100.f};
};

void editor_camera_t::update_projection(float aspect_ratio) {
    static float s_aspect_ratio = 0;
    if (s_aspect_ratio != aspect_ratio) {
        _projection = glm::perspective(glm::radians(fov), aspect_ratio, near, far);
        s_aspect_ratio = aspect_ratio;
    }
}

void editor_camera_t::update(float ts) {
    auto [width, height] = _window.dimensions();
    update_projection(float(width) / float(height));

    double curX, curY;
    glfwGetCursorPos(_window.window(), &curX, &curY);

    float velocity = _mouse_speed * ts;

    if (glfwGetKey(_window.window(), GLFW_KEY_W)) 
        _position += _front * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_S)) 
        _position -= _front * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_D)) 
        _position += _right * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_A)) 
        _position -= _right * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_SPACE)) 
        _position += _up * velocity;
    if (glfwGetKey(_window.window(), GLFW_KEY_LEFT_SHIFT)) 
        _position -= _up * velocity;
    
    glm::vec2 mouse{curX, curY};
    glm::vec2 difference = mouse - _initial_mouse;
    _initial_mouse = mouse;

    if (glfwGetMouseButton(_window.window(), GLFW_MOUSE_BUTTON_1)) {
        
        difference.x = difference.x / float(width);
        difference.y = -(difference.y / float(height));

        _yaw += difference.x * _mouse_sensitivity;
        _pitch += difference.y * _mouse_sensitivity;

        if (_pitch > 89.0f) 
            _pitch = 89.0f;
        if (_pitch < -89.0f) 
            _pitch = -89.0f;
    }

    glm::vec3 front;
    front.x = glm::cos(glm::radians(_yaw)) * glm::cos(glm::radians(_pitch));
    front.y = glm::sin(glm::radians(_pitch));
    front.z = glm::sin(glm::radians(_yaw)) * glm::cos(glm::radians(_pitch));
    _front = front * camera_speed_multiplyer;
    _right = glm::normalize(glm::cross(_front, glm::vec3{0, 1, 0}));
    _up    = glm::normalize(glm::cross(_right, _front));

    _view = glm::lookAt(_position, _position + _front, glm::vec3{0, 1, 0});
}

glm::vec3 createRay(glm::vec2 px, glm::mat4 PInv, glm::mat4 VInv) {
	 
	// convert pixel to NDS
	// [0,1] -> [-1,1]
	glm::vec2 pxNDS = px*2.f - 1.f;

	// choose an arbitrary point in the viewing volume
	// z = -1 equals a point on the near plane, i.e. the screen
	glm::vec3 pointNDS = glm::vec3(pxNDS, -1.f);

	// as this is in homogenous space, add the last homogenous coordinate
	glm::vec4 pointNDSH = glm::vec4(pointNDS, 1.0);
	// transform by inverse projection to get the point in view space
	glm::vec4 dirEye = PInv * pointNDSH;

	// since the camera is at the origin in view space by definition,
	// the current point is already the correct direction (dir(0,P) = P - 0 = P
	// as a direction, an infinite point, the homogenous component becomes 0
	// the scaling done by the w-division is not of interest, as the direction
	// in xyz will stay the same and we can just normalize it later
	dirEye.w = 0.f;

	// compute world ray direction by multiplying the inverse view matrix
	glm::vec3 dirWorld = glm::vec3(VInv * dirEye);

	// now normalize direction
	return glm::normalize(dirWorld); 
}

int main() {
    log_t::set_log_level(log_level_t::e_info);

    auto model = core::load_model_from_path("../../assets/models/quad/quad.obj");
    std::vector<triangle_t> triangles;
    {
        for (auto& mesh : model.meshes) {
            check(mesh.indices.size() % 3 == 0, "indices count is not a multiple of 3");
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                core::vertex_t v0 = mesh.vertices[mesh.indices[i + 0]];
                core::vertex_t v1 = mesh.vertices[mesh.indices[i + 1]];
                core::vertex_t v2 = mesh.vertices[mesh.indices[i + 2]];

                triangle_t triangle;
                triangle.p0 = v0.position;
                triangle.p1 = v1.position;
                triangle.p2 = v2.position;

                triangles.push_back(triangle);
            }
        }
    }
    horizon_info("Loaded file with {} triangle(s)", triangles.size());
    horizon_info("Triangles");
    for (auto& triangle : triangles) {
        horizon_info("p0: {} p1: {} p2: {}", glm::to_string(triangle.p0), glm::to_string(triangle.p1), glm::to_string(triangle.p2));
    }
    
    std::vector<bounding_box_t> aabbs(triangles.size());
    std::vector<glm::vec3> centers(triangles.size());
    
    for (size_t i = 0; i < triangles.size(); i++) {
        aabbs[i] = bounding_box_t{}.extend(triangles[i].p0).extend(triangles[i].p1).extend(triangles[i].p2);
        centers[i] = (triangles[i].p0 + triangles[i].p1 + triangles[i].p2) / 3.0f;
    }

    bvh_t bvh = bvh_t::build(aabbs.data(), centers.data(), triangles.size());

    horizon_info("Built BVH with {} node(s) and depth {}", bvh.nodes.size(), bvh.depth());

    

    // horizon_info("{} {}", glm::to_string(editor_camera.position()), glm::to_string(editor_camera.front()));
    // horizon_info("{}", glm::to_string(editor_camera.view()));

    // {
    //     ray_t ray{};
    //     ray.tmin = 0;
    //     ray.tmax = std::numeric_limits<float>::max();

    //     glm::vec3 position = {0, 0, -5};
    //     glm::vec3 front = {0, 0, 1};
    //     glm::mat4 view = glm::lookAt(position, position + front, glm::vec3{0, 1, 0});
    //     glm::mat4 projection = glm::perspective(glm::radians(45.f), 1.52380955f, 0.100000001f, 1000.f);


    //     ray.direction = createRay(glm::vec2{0.5f, 0.5f}, glm::inverse(projection), glm::inverse(view));
    //     ray.origin = position;

    //     horizon_info("{}", glm::to_string(ray.direction));

    //     if (triangles[0].intersect(ray)) {
    //         horizon_info("hit");
    //     } else {
    //         horizon_info("not hit");
    //     }
    // }

    glm::vec3 eye{ 0, 0, 1 };
    glm::vec3 dir{ 0, 0, -1 };
    glm::vec3 up{ 0, 1, 0 };

    // ray_t ray{};
    // ray.origin = eye;
    // ray.direction = dir;
    // ray.tmin = 0;
    // ray.tmax = std::numeric_limits<float>::max();
    // auto hit = bvh.traverse(ray, triangles);
    // if (hit) {
    //     horizon_info("hit");
    // } else {
    //     horizon_info("not hit");
    // }

    auto right = glm::normalize(glm::cross(glm::normalize(dir), glm::normalize(up)));
    up = glm::cross(right, glm::normalize(dir));

    screen_t screen{ 640, 640 };
    for (size_t y = 0; y < screen._height; y++) for (size_t x = 0; x < screen._width; x++) {
        auto u = 2.0f * static_cast<float>(x) / static_cast<float>(screen._width) - 1.0f;
        auto v = 2.0f * static_cast<float>(y) / static_cast<float>(screen._height) - 1.0f;
        // u: 0.16562498 v: 0.16562498
        // u = 0.16562498;
        // v = 0.16562498;
        ray_t ray;
        ray.origin = eye;
        ray.direction = dir + (u * right) + (v * up);
        ray.tmin = 0;
        ray.tmax = std::numeric_limits<float>::max();
        auto hit = bvh.traverse(ray, triangles);
        // if (hit) 
        // if (hit) {
        //     horizon_info("u: {} v: {}", u, v);
        // }
        screen.plot(x, y, pixel_t{ static_cast<uint8_t>(hit.primitive_index * 37), static_cast<uint8_t>(hit.primitive_index * 91), static_cast<uint8_t>(hit.primitive_index * 51) });
        // horizon_info("{}", hit.primitive_index);
    }

    screen.to_disk("test.ppm");

    return 0;
}