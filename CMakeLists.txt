cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()


project(horizon)

add_subdirectory(horizon)
add_subdirectory(deps)
add_subdirectory(projects)

# set(CMAKE_VERBOSE_MAKEFILE ON)
