cmake_minimum_required(VERSION 3.15)

project(raytracing)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(raytracing ${CPP_SRC_FILES})

target_link_libraries(raytracing
	PUBLIC slang
	PUBLIC horizon
)

target_include_directories(raytracing
	PUBLIC slang
)
