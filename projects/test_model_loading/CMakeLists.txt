cmake_minimum_required(VERSION 3.10)

project(test_model_loading)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME test_model_loading)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(test_model_loading ${SRC_FILES})

include_directories(test_model_loading
    ../../horizon
    .
)

target_link_libraries(test_model_loading
    horizon
)