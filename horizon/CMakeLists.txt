cmake_minimum_required(VERSION 3.10)

project(horizon)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

add_library(horizon ${SRC_FILES})

find_package(Vulkan REQUIRED)

find_package(X11 REQUIRED)
if(!X11_XTest_FOUND)
    message(FATAL_ERROR "X11 extensions are required, but not found!")
endif()
if(!X11_Xfixes_LIB)
    message(FATAL_ERROR "X11 fixes extension is required, but not found!")
endif()
if (!X11_Xext_LIB)
    message(FATAL_ERROR "X11 Xext extension required but not found!")
endif()

target_compile_definitions(horizon PUBLIC
    VIZON_PROFILE_ENABLE
)

include_directories(horizon
    Vulkan::Headers
    ../deps
    ../deps/imgui
    .
)

target_include_directories(horizon
    PUBLIC glslang
    PUBLIC SPIRV-Headers
    PUBLIC SPIRV-Tools
    PUBLIC shaderc
    PUBLIC glslc
    PUBLIC shaderc_util
)

target_link_libraries(horizon
    glfw
    glm
    stb_image
    spdlog
    volk
    vk-bootstrap
    glslang
    SPIRV-Headers
    SPIRV-Tools
    shaderc
    glslc
    shaderc_util
    VulkanMemoryAllocator
    # EnTT
    # assimp
    # screen_capture_lite_static
    # imgui
    # msdf-atlas-gen
)