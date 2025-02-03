cmake_minimum_required(VERSION 3.15)

project(radix_sort)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(radix_sort ${CPP_SRC_FILES})

target_link_libraries(radix_sort
	PUBLIC slang
	PUBLIC horizon
)

target_include_directories(radix_sort
	PUBLIC slang
)
