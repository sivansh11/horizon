file(GLOB_RECURSE CPP_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(horizon ${CPP_SRC_FILES})

target_link_libraries(horizon
  PUBLIC stb_cmake
  PUBLIC glm
  PUBLIC glfw
  PUBLIC assimp
  PUBLIC volk
  PUBLIC vk-bootstrap
  PUBLIC VulkanMemoryAllocator
  PUBLIC slang
  PUBLIC imgui_cmake
)

target_include_directories(horizon
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
  PUBLIC slang
  PUBLIC vk-bootstrap
)
