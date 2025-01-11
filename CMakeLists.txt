cmake_minimum_required(VERSION 3.22)

project(horizon)

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_VERBOSE_MAKEFILE ON)

# # options
option(HORIZON_BUILD_EXAMPLES "build EXAMPLES" OFF)


# # deps
set(FETCHCONTENT_QUIET FALSE)

set(SHADERC_SKIP_TESTS ON CACHE INTERNAL "Turn off shaderc testing")
set(ASSIMP_INSTALL OFF CACHE INTERNAL "turns off assimp install")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE INTERNAL "assimp warnings as error")
set(IMGUI_VULKAN_BACKEND ON CACHE INTERNAL "Imgui vulkan backend")
set(IMGUI_GLFW_BACKEND ON CACHE INTERNAL "Imgui glfw backend")
set(IMGUI_IMPL_VULKAN_USE_LOADER ON CACHE INTERNAL "Imgui vulkan use loader")
set(IMGUI_IMPL_VULKAN_NO_PROTOTYPES ON CACHE INTERNAL "Imgui vulkan no prototypes")

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
)
FetchContent_Declare(
  assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG a4d8a5fec3fc42b17c6411b68035017c7d797298
)
FetchContent_Declare(
  volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
)
FetchContent_Declare(
  vk-bootstrap
  GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
  GIT_TAG main
)
FetchContent_Declare(
  VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
)
FetchContent_Declare(
  imgui_cmake
  GIT_REPOSITORY https://github.com/sivansh11/imgui_cmake.git
  GIT_TAG main
  # GIT_TAG c63ccd36f8c843270a5184f3016971d729bbabf8
)
FetchContent_Declare(
  slang
  GIT_REPOSITORY https://github.com/shader-slang/slang.git
  GIT_TAG 772c48af91951bda9ff7064fc7a089550c759f76
)
FetchContent_Declare(
  shaderc
  GIT_REPOSITORY https://github.com/google/shaderc.git
  GIT_TAG 082a73bfa9088bd3da35734023a9167ba92aae6c
)

if (NOT glm_POPULATED)
  FetchContent_MakeAvailable(glm)
endif()
if (NOT glfw_POPULATED)
  FetchContent_MakeAvailable(glfw)
  set (IMGUI_GLFW_PATH ${glfw_SOURCE_DIR}/include)
endif()
if (NOT assimp_POPULATED)
  FetchContent_MakeAvailable(assimp)
endif()
if (NOT volk_POPULATED)
  FetchContent_MakeAvailable(volk)
endif()
if (NOT vk-bootstrap_POPULATED)
  FetchContent_MakeAvailable(vk-bootstrap)
endif()
if (NOT VulkanMemoryAllocator_POPULATED)
  FetchContent_MakeAvailable(VulkanMemoryAllocator)
endif()
if (NOT imgui_cmake_POPULATED)
  FetchContent_MakeAvailable(imgui_cmake)
endif()
if (NOT slang_POPULATED)
  FetchContent_MakeAvailable(slang)
endif()
if (NOT shaderc_POPULATED)
  FetchContent_MakeAvailable(shaderc)
endif()

# # src
add_subdirectory(src)


if (HORIZON_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()


# best practices, read https://cliutils.gitlab.io/modern-cmake/chapters/basics/structure.html bottom of the page
# # # Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
  message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

# default built type if CMAKE_BUILD_TYPE is not set
set(DEFAULT_BUILT_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILT_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILT_TYPE}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
