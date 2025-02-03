cmake_minimum_required(VERSION 3.15)

project(chip-8)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(chip-8 ${CPP_SRC_FILES})

# target_compile_options(chip-8 PUBLIC -fsanitize=address)
# target_link_options(chip-8 PUBLIC -fsanitize=address)
# target_compile_options(chip-8 PUBLIC -fsanitize=undefined)
# target_link_options(chip-8 PUBLIC -fsanitize=undefined)

target_link_libraries(chip-8
	PUBLIC slang
	PUBLIC horizon
#   PUBLIC lsan
)

target_include_directories(chip-8
	PUBLIC slang
)
