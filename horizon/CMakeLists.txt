cmake_minimum_required(VERSION 3.10)

project(horizon)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

add_library(horizon ${SRC_FILES})

target_compile_definitions(horizon PUBLIC
    # horizon_profile_enable
)

include_directories(horizon
    Vulkan::Headers
    ../deps
    ../deps/imgui_cmake
    ../deps/imgui_cmake/imgui
    .
)

target_include_directories(horizon
    PUBLIC glslang
    PUBLIC SPIRV-Headers
    PUBLIC SPIRV-Tools
    PUBLIC shaderc
    PUBLIC glslc
    PUBLIC shaderc_util
    PUBLIC slang
    PUBLIC core
    PUBLIC gfx
    PUBLIC imgui_cmake
)

target_link_libraries(horizon
    glfw
    glm
    stb_image
    volk
    vk-bootstrap
    glslang
    SPIRV-Headers
    SPIRV-Tools
    shaderc
    glslc
    shaderc_util
    VulkanMemoryAllocator
    assimp
    slang
    core
    imgui_cmake
)