cmake_minimum_required(VERSION 3.10)

project(bindless_test)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME bindless_test)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(bindless_test ${SRC_FILES})

include_directories(bindless_test
    ../../horizon
    .
)

target_link_libraries(bindless_test
    horizon
)