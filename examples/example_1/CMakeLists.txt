cmake_minimum_required(VERSION 3.15)

project(example_1)

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(example_1 ${CPP_SRC_FILES})

target_link_libraries(example_1
	PUBLIC horizon
)

target_include_directories(example_1
	PUBLIC horizon
)
