cmake_minimum_required(VERSION 3.10)

project(load_image_test)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME load_image_test)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(load_image_test ${SRC_FILES})

include_directories(load_image_test
    ../../horizon
    .
)

target_link_libraries(load_image_test
    horizon
)