file(GLOB_RECURSE CPP_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

if (HORIZON_INCLUDE_IMGUI)
  target_compile_definitions(horizon PUBLIC HORIZON_INCLUDE_IMGUI)
endif()

add_library(horizon ${CPP_SRC_FILES})

target_link_libraries(horizon
  PUBLIC math
  PUBLIC stb_cmake
  PUBLIC glfw
  PUBLIC volk
  PUBLIC vk-bootstrap
  PUBLIC VulkanMemoryAllocator
  PUBLIC slang
)
if (HORIZON_INCLUDE_BVH)
  target_link_libraries(horizon bvh)
endif()
if (HORIZON_INCLUDE_ENTT)
  target_link_libraries(horizon EnTT::EnTT)
endif()
if (HORIZON_INCLUDE_IMGUI)
  target_link_libraries(horizon imgui_cmake)
endif()

target_include_directories(horizon
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../includes
  PUBLIC slang
  PUBLIC vk-bootstrap
)
