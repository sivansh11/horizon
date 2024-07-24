cmake_minimum_required(VERSION 3.10)

project(bindless_manager)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME bindless_manager)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(bindless_manager ${SRC_FILES})

include_directories(bindless_manager
    ../../horizon
    .
)

target_link_libraries(bindless_manager
    horizon
)