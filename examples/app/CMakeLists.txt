cmake_minimum_required(VERSION 3.15)

project(app)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(app ${CPP_SRC_FILES})

target_link_libraries(app
	PUBLIC slang
	PUBLIC horizon
)

target_include_directories(app
	PUBLIC slang
)
