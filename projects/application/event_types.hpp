#ifndef EVENT_TYPES_HPP
#define EVENT_TYPES_HPP

#include "gfx/context.hpp"

#include <cstdint>

struct viewport_resize_t {
    uint32_t width, height;
};

struct set_viewport_image_t {
    gfx::handle_image_view_t image_view;
    gfx::handle_sampler_t    sampler;    // this needs to be the same as the base renderer sampler/global sampler
};

#endif