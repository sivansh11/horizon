cmake_minimum_required(VERSION 3.15)

project(demo)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")
set(FETCHCONTENT_QUIET FALSE)

include(FetchContent)

FetchContent_Declare(
  model
  GIT_REPOSITORY https://github.com/sivansh11/model
  GIT_TAG main
)
FetchContent_MakeAvailable(model)

file(GLOB_RECURSE CPP_SRC_FILES ./*.cpp)

add_executable(demo ${CPP_SRC_FILES})

target_link_libraries(demo
	PUBLIC horizon
	PUBLIC model
)

target_include_directories(demo
	PUBLIC horizon
)
