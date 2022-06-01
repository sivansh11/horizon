cmake_minimum_required(VERSION 3.10)

project(horizon)

add_subdirectory(deps/glfw)
add_subdirectory(deps/tinyobjloader)
add_subdirectory(deps/glm)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_executable(horizon ${SRC_FILES})

target_include_directories(horizon
    PUBLIC src
)

target_link_libraries(horizon
    glfw
    glm
    tinyobjloader
    vulkan
    dl
    pthread
    X11
    Xxf86vm
    Xrandr
    Xi
)

##############################################################
# SHADERS
##############################################################

# find_program(GLSL_VALIDATOR glslangValidator HINTS 
#   ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} 
#   /usr/bin 
#   /usr/local/bin 
#   ${VULKAN_SDK_PATH}/Bin
#   ${VULKAN_SDK_PATH}/Bin32
#   $ENV{VULKAN_SDK}/Bin/ 
#   $ENV{VULKAN_SDK}/Bin32/
# )

# # get all .vert and .frag files in shaders directory
# file(GLOB_RECURSE GLSL_SOURCE_FILES
#   "${PROJECT_SOURCE_DIR}/shaders/*.frag"
#   "${PROJECT_SOURCE_DIR}/shaders/*.vert"
# )

# foreach(GLSL ${GLSL_SOURCE_FILES})
#   get_filename_component(FILE_NAME ${GLSL} NAME)
#   set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
#   add_custom_command(
#     OUTPUT ${SPIRV}
#     COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
#     DEPENDS ${GLSL})
#   list(APPEND SPIRV_BINARY_FILES ${SPIRV})
# endforeach(GLSL)

# add_custom_target(
#     Shaders
#     DEPENDS ${SPIRV_BINARY_FILES}
# )