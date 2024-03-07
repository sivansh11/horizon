import os

# # name link

submodules = [
    # ("deps/tinyxml2", "https://github.com/leethomason/tinyxml2.git"),
    ("deps/glm", "https://github.com/g-truc/glm.git"),
    # ("deps/shaderc", "https://github.com/google/shaderc.git"),
    # ("deps/SPIRV-Tools", "https://github.com/KhronosGroup/SPIRV-Tools.git"),
    ("deps/glfw", "https://github.com/glfw/glfw.git"),
    # ("deps/glslang", "https://github.com/KhronosGroup/glslang.git"),
    # ("deps/imgui/imgui", "https://github.com/ocornut/imgui.git"),
    ("deps/volk", "https://github.com/zeux/volk.git"),
    ("deps/spdlog", "https://github.com/gabime/spdlog.git"),
    # ("deps/SPIRV-Headers", "https://github.com/KhronosGroup/SPIRV-Headers.git"),
    # ("deps/msdf-atlas-gen", "https://github.com/Chlumsky/msdf-atlas-gen.git"),
]


for path, link in submodules:
    # print(f'rm -r -f {path}')
    # os.system(f'rm -r -f {path}')
    print(f'git submodule add {link} {path}')
    os.system(f'git submodule add {link} {path}')
    