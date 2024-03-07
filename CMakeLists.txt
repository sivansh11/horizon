cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

project(horizon)

add_subdirectory(deps)
add_subdirectory(horizon)
add_subdirectory(projects)

set(CMAKE_VERBOSE_MAKEFILE ON)
