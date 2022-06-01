cmake_minimum_required(VERSION 3.10)
project(tinyobjloader)
add_library(tinyobjloader src/tinyobjloader.cpp)
target_include_directories(tinyobjloader
    PUBLIC include
)