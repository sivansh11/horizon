cmake_minimum_required(VERSION 3.10)

project(rt_debug)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME rt_debug)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(rt_debug ${SRC_FILES})

include_directories(rt_debug
    ../../horizon
    .
)

target_link_libraries(rt_debug
    horizon
)