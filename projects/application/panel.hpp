#ifndef PANEL_HPP
#define PANEL_HPP

#include "core/core.hpp"
#include "core/window.hpp"
#include "core/ecs.hpp"
#include "core/event.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "event_types.hpp"
#include "panel.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct debug_panel_t {
    void render() {
        if (!show) return;
        if (!ImGui::Begin(name.c_str(), &show)) {
            ImGui::End();
            return;
        }

        ImGui::Text("%f", ImGui::GetIO().Framerate);
        ImGui::End();
    }
    std::string name = "Debug";
    bool show = true;
};

struct viewport_panel_t {
    viewport_panel_t(core::dispatcher_t& dispatcher, gfx::context_t& context) : _dispatcher(dispatcher), _context(context) {
        gfx::config_descriptor_set_layout_t cdsl{ .use_bindless = false };
        cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        dsl = _context.create_descriptor_set_layout(cdsl);
        ds = _context.allocate_descriptor_set({ .handle_descriptor_set_layout = dsl, .debug_name = "viewport descriptor set"});
        _dispatcher.subscribe<set_viewport_image_t>([this](const core::event_t& e) {
            const set_viewport_image_t& event = reinterpret_cast<const set_viewport_image_t&>(e);
            set_image(event.image_view, event.sampler);
        });
    }
    
    void set_image(gfx::handle_image_view_t image_view, gfx::handle_sampler_t sampler) {
        _context.update_descriptor_set(ds)
            .push_image_write(0, { .handle_sampler = sampler, .handle_image_view = image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL })
            .commit();
    }

    void render() {
        if (!show) return;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGuiWindowClass window_class;
        window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::SetNextWindowClass(&window_class);
        ImGuiWindowFlags viewPortFlags = ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoDecoration;
        if (!ImGui::Begin(name.c_str(), &show, viewPortFlags)) {
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        }
        auto [vp_w, vp_h] = ImGui::GetContentRegionAvail();
        if (vp_w != viewport_width || vp_h != viewport_height) {
            _dispatcher.post<viewport_resize_t>(static_cast<uint32_t>(vp_w), static_cast<uint32_t>(vp_h));
        }
        ImGui::Image(reinterpret_cast<ImTextureID>(reinterpret_cast<void *>(_context.get_descriptor_set(ds).vk_descriptor_set)), { vp_w, vp_h });
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
    core::dispatcher_t& _dispatcher;
    gfx::context_t& _context;
    gfx::handle_descriptor_set_layout_t dsl;
    gfx::handle_descriptor_set_t ds;
    uint32_t viewport_width = 5, viewport_height = 5;
    gfx::handle_image_t image;
    std::string name = "Viewport";
    bool show = true;
};

#endif