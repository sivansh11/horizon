cmake_minimum_required(VERSION 3.15)

project(rendergraph)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(rendergraph ${CPP_SRC_FILES})

target_link_libraries(rendergraph
	PUBLIC horizon
)

target_include_directories(rendergraph
	PUBLIC horizon
)
