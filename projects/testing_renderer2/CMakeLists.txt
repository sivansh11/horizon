cmake_minimum_required(VERSION 3.10)

project(testing_renderer2)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME testing_renderer2)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(testing_renderer2 ${SRC_FILES})

include_directories(testing_renderer2
    ../../horizon
    .
)

target_link_libraries(testing_renderer2
    horizon
)