cmake_minimum_required(VERSION 3.10)

project(test_rt)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME test_rt)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(test_rt ${SRC_FILES})

include_directories(test_rt
    ../../horizon
    .
)

target_link_libraries(test_rt
    horizon
)