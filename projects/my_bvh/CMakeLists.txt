cmake_minimum_required(VERSION 3.10)

project(my_bvh)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME my_bvh)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(my_bvh ${SRC_FILES})

include_directories(my_bvh
    ../../horizon
    .
)

target_link_libraries(my_bvh
    horizon
)