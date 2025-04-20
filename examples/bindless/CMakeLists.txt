cmake_minimum_required(VERSION 3.15)

project(bindless)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(bindless ${CPP_SRC_FILES})

target_link_libraries(bindless
	PUBLIC horizon
)

target_include_directories(bindless
	PUBLIC horizon
)
