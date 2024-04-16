cmake_minimum_required(VERSION 3.10)

project(something)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME something)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(something ${SRC_FILES})

include_directories(something
    ../../horizon
    .
)

target_link_libraries(something
    horizon
)