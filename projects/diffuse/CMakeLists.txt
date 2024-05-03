cmake_minimum_required(VERSION 3.10)

project(diffuse)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME diffuse)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(diffuse ${SRC_FILES})

include_directories(diffuse
    ../../horizon
    .
)

target_link_libraries(diffuse
    horizon
)