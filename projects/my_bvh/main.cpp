#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#include "display.hpp"
#include "bvh.hpp"
// #include "new_bvh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::rng_t rng{ 0 };

    auto model = core::load_model_from_path("../../assets/models/teapot.obj");
    std::vector<bvh::triangle_t> triangles;
    {
        for (auto& mesh : model.meshes) {
            check(mesh.indices.size() % 3 == 0, "indices count is not a multiple of 3");
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                core::vertex_t v0 = mesh.vertices[mesh.indices[i + 0]];
                core::vertex_t v1 = mesh.vertices[mesh.indices[i + 1]];
                core::vertex_t v2 = mesh.vertices[mesh.indices[i + 2]];

                bvh::triangle_t triangle;
                triangle.p0 = v0.position;
                triangle.p1 = v1.position;
                triangle.p2 = v2.position;

                triangle.color = rng.random3f();

                triangles.push_back(triangle);
            }
        }
    }
    horizon_info("Loaded file with {} triangle(s)", triangles.size());
    
    std::vector<bvh::primitive_t> primitives(triangles.size());
    
    for (size_t i = 0; i < triangles.size(); i++) {
        primitives[i].aabb = bvh::aabb_t{}.extend(triangles[i].p0).extend(triangles[i].p1).extend(triangles[i].p2);
        primitives[i].center = (triangles[i].p0 + triangles[i].p1 + triangles[i].p2) / 3.0f;
        primitives[i].triangle = triangles[i];
    }

    bvh::bvh_t bvh{ primitives };

    horizon_info("node_count : {}", bvh.node_count());

    glm::vec3 eye{ 0, 0, 10 };
    glm::vec3 dir{ 0, 0, -1 };
    glm::vec3 up{ 0, 1, 0 };

    screen_t screen{ 640, 640 };

    auto right = glm::normalize(glm::cross(glm::normalize(dir), glm::normalize(up)));
    up = glm::cross(right, glm::normalize(dir));

    {
        core::timer::scope_timer_t timer{ [](core::timer::duration_t duration) {
            std::cout << duration << '\n';
        }};
        float avj_node_intersections = 0;
        float avj_primitive_intersections = 0;

        for (size_t y = 0; y < screen._height; y++) {

            for (size_t x = 0; x < screen._width; x++) {
                auto u = 2.0f * static_cast<float>(x) / static_cast<float>(screen._width) - 1.0f;
                auto v = 2.0f * static_cast<float>(y) / static_cast<float>(screen._height) - 1.0f;

                bvh::ray_t ray;
                ray.origin = eye;
                ray.direction = dir + (u * right) + (v * up);
                ray.tmin = 0;
                ray.tmax = std::numeric_limits<float>::max();
                auto [hit, node_intersections, primitive_intersections] = bvh.stack_traverse(ray);
                
                avj_node_intersections += node_intersections;
                avj_primitive_intersections += primitive_intersections;
                
                if (hit.primitive) {
                    screen.plot(x, y, { static_cast<uint8_t>(hit.primitive.value().triangle.color.x * 255), static_cast<uint8_t>(hit.primitive.value().triangle.color.y * 255), static_cast<uint8_t>(hit.primitive.value().triangle.color.z * 255), 255 });
                } else {
                    screen.plot(x, y, { 0, 0, 0, 255 });
                }
                // std::cerr << y << '\n';
            }
        }
        horizon_info("{} {} {}", avj_node_intersections / float(screen._width * screen._height), avj_primitive_intersections / float(screen._width * screen._height), avj_node_intersections / float(screen._width * screen._height) + avj_primitive_intersections / float(screen._width * screen._height));
    }


    screen.to_disk("test.ppm");
    return 0;
}