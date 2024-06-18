import os
import sys

project_cmake_src_template = """cmake_minimum_required(VERSION 3.10)

project([[[name]]])

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME [[[name]]])
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable([[[name]]] ${SRC_FILES})

include_directories([[[name]]]
    ../../horizon
    .
)

target_link_libraries([[[name]]]
    horizon
)"""


main_file_template = '''#include "test.hpp"

int main(int argc, char **argv) {
    test::test();
    return 0;
}
'''

if len(sys.argv) != 2:
    print("python add_new_project.py {name}")
    exit(1)
    
name = sys.argv[1]

if os.path.exists(f'projects/{name}'):
    print(f'{name} project already exist')
    exit(1)

os.mkdir(f'projects/{name}')

with open(f'projects/{name}/CMakeLists.txt', 'w') as project_cmake_file:
    project_cmake_file.write(project_cmake_src_template.replace('[[[name]]]', name))

with open(f'projects/{name}/main.cpp', 'w') as main_file:
    main_file.write(main_file_template)

with open(f'projects/CMakeLists.txt', 'a') as projects_cmake_file:
    projects_cmake_file.write(f'\nadd_subdirectory({name})')



# print(sys.argv)