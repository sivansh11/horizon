#ifndef UTILITY_HPP
#define UTILITY_HPP

#include "core/model.hpp"
#include "gfx/base_renderer.hpp"

inline std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height) {
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = { width, height };
    return { viewport, scissor };
}

#endif