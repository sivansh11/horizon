#include "horizon/core/ecs.hpp"
#include <array>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {

  ecs::scene_t<2> scene{};

  for (uint32_t i = 0; i < 6; i++) {
    auto id = scene.create();
      scene.construct<int>(id) = i;
  }

  scene.destroy(2);
  scene.destroy(3);

  // for (uint32_t i = 0; i < 64; i++) {
    // auto id = scene.create();
    // if (i % 2 == 0) {
      // scene.construct<int>(id) = i;
    // } else {
      // scene.construct<float>(id) = i;
    // }
  // }
// 
  // scene.for_all<>([&](ecs::entity_id_t id) {
    // if (id % 4 == 0) {
      // scene.destroy(id);
    // }
  // });
// 
  // // scene.for_all<>([&](ecs::entity_id_t id){
  // //   if (id % 3 == 0) {
  // //     scene.destroy(id);
  // //   }
  // // });
// 
// scene.for_all<int>([](auto, auto v) { std::cout << v << '\n'; });
// 
// scene.for_all<float>([](auto, auto v) { std::cout << v << '\n'; });

  return 0;
}
