cmake_minimum_required(VERSION 3.10)

project(application)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME application)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(application ${SRC_FILES})

include_directories(application
    ../../horizon
    .
)

target_link_libraries(application
    horizon
)