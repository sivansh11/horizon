cmake_minimum_required(VERSION 3.15)

project(example_0)

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(example_0 ${CPP_SRC_FILES})

target_link_libraries(example_0
	PUBLIC slang
	PUBLIC horizon
)

target_include_directories(example_0
	PUBLIC slang
)
