cmake_minimum_required(VERSION 3.15)

project(bvh)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(bvh ${CPP_SRC_FILES})

# target_compile_options(bvh PUBLIC -fsanitize=address)
# target_link_options(bvh PUBLIC -fsanitize=address)
# target_compile_options(bvh PUBLIC -fsanitize=undefined)
# target_link_options(bvh PUBLIC -fsanitize=undefined)


target_link_libraries(bvh
	PUBLIC slang
	PUBLIC horizon
)

target_include_directories(bvh
	PUBLIC slang
)
