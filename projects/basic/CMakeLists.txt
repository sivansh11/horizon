cmake_minimum_required(VERSION 3.10)

project(basic)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME basic)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(basic ${SRC_FILES})

include_directories(basic
    ../../horizon
    .
)

target_link_libraries(basic
    horizon
)