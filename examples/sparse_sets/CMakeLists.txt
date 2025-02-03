cmake_minimum_required(VERSION 3.15)

project(sparse_sets)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(sparse_sets ${CPP_SRC_FILES})

target_link_libraries(sparse_sets
	PUBLIC slang
	PUBLIC horizon
)

target_include_directories(sparse_sets
	PUBLIC slang
)
