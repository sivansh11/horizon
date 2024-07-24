#include "core/core.hpp"
#include "core/window.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include <GLFW/glfw3.h>

#include <unordered_map>

#define MAX_SLOTS 1000

struct image_t {
    gfx::handle_image_t image;
    gfx::handle_image_view_t image_view;

    bool operator==(const image_t& other) const = default;
};

template <>
struct std::hash<image_t> {
    size_t operator()(const image_t& image) const {
        size_t seed = 0;
        core::hash_combine(seed, image.image.val, image.image_view.val);
        return seed;
    }
};

struct bindless_manager_t {
    bindless_manager_t(gfx::context_t& context) : _context(context) {
        gfx::handle_command_pool_t command_pool = _context.create_command_pool({});

        gfx::config_descriptor_set_layout_t cdsl{};
        cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL, MAX_SLOTS);
        _dsl = _context.create_descriptor_set_layout(cdsl);
        _ds  = _context.allocate_descriptor_set({ .handle_descriptor_set_layout = _dsl });
        _default_image = gfx::helper::load_image_from_path(_context, command_pool, "../../assets/texture/default.png", VK_FORMAT_R8G8B8A8_SRGB);
        _default_image_view = _context.create_image_view({ .handle_image = _default_image });
        slot({ _default_image, _default_image_view });
        _context.destroy_command_pool(command_pool);
    }

    ~bindless_manager_t() {
        _context.destroy_image_view(_default_image_view);
        _context.destroy_image(_default_image);
        _context.free_descriptor_set(_ds);
        _context.destroy_descriptor_set_layout(_dsl);
    }

    uint64_t slot(const image_t& image) {
        uint64_t slot;
        auto itr = image_slot.find(image);
        if (itr == image_slot.end()) {
            slot = image_slot.size();
            image_slot[image] = slot;
        } else {
            slot = itr->second;
        }
        assert(slot < MAX_INPUT);
        gfx::image_descriptor_info_t image_update{ .handle_image_view = image.image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };  // assuming shader read only optimal
        image_updates.push_back({ image_update, slot });
        return slot;
    }

    void flush_updates() {
        auto update_ds = _context.update_descriptor_set(_ds);
        for (auto& [image_update, slot] : image_updates) {
            update_ds.push_image_write(0, image_update, slot);
        }
        update_ds.commit();
    }

    gfx::context_t& _context;

    gfx::handle_descriptor_set_layout_t _dsl;
    gfx::handle_descriptor_set_t _ds;

    gfx::handle_image_t _default_image;
    gfx::handle_image_view_t _default_image_view;

    std::unordered_map<image_t, uint64_t> image_slot;

    std::vector<std::pair<gfx::image_descriptor_info_t, uint64_t>> image_updates;
};

int main(int argc, char **argv) {
    core::window_t window{ "bindless", 640, 640 };
    gfx::context_t context{ true };

    auto [width, height] = window.dimensions();
    auto [target_image, target_image_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto sampler = context.create_sampler({});

    renderer::base_renderer_t renderer{ window, context, sampler, target_image_view };
    bindless_manager_t bindless_manager{ context };

    

    bindless_manager.flush_updates();

    return 0;
}
