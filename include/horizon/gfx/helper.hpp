#ifndef GFX_HELPER_HPP
#define GFX_HELPER_HPP

#include "context.hpp"

namespace gfx {

namespace helper {

std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height);

}

}

#endif
