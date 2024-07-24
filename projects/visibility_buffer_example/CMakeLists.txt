cmake_minimum_required(VERSION 3.10)

project(visibility_buffer_example)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME visibility_buffer_example)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(visibility_buffer_example ${SRC_FILES})

include_directories(visibility_buffer_example
    ../../horizon
    .
)

target_link_libraries(visibility_buffer_example
    horizon
)