cmake_minimum_required(VERSION 3.10)

project(ppm_viewer)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME ppm_viewer)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(ppm_viewer ${SRC_FILES})

include_directories(ppm_viewer
    ../../horizon
    .
)

target_link_libraries(ppm_viewer
    horizon
)