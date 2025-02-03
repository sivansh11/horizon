#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "chip-8.hpp"

#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/types.hpp"
#include "imgui.h"
#include "imgui_memory_editor.h"
#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan_core.h>

struct renderer_t {
  renderer_t(core::window_t &window, bool validation)
      : _window(window), _is_validating(validation) {
    _ctx = core::make_ref<gfx::context_t>(_is_validating);

    _swapchain_sampler = _ctx->create_sampler({});

    auto [width, height] = _window.dimensions();

    gfx::config_image_t config_final_image{};
    config_final_image.vk_width = width;
    config_final_image.vk_height = height;
    config_final_image.vk_depth = 1;
    config_final_image.vk_type = VK_IMAGE_TYPE_2D;
    config_final_image.vk_format = VK_FORMAT_R8G8B8A8_UNORM;
    config_final_image.vk_usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    config_final_image.vk_mips = 1;
    config_final_image.debug_name = "final image";
    _final_image = _ctx->create_image(config_final_image);
    _final_image_view = _ctx->create_image_view({.handle_image = _final_image});

    gfx::base_config_t base_config{.window = _window,
                                   .context = *_ctx,
                                   .sampler = _swapchain_sampler,
                                   .final_image_view = _final_image_view};
    _base = core::make_ref<gfx::base_t>(base_config);

    gfx::helper::imgui_init(_window, *_ctx, _base->_swapchain,
                            VK_FORMAT_R8G8B8A8_UNORM);

    gfx::config_descriptor_set_layout_t
        config_chip8_image_descriptor_set_layout{};
    config_chip8_image_descriptor_set_layout.add_layout_binding(
        0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT);
    config_chip8_image_descriptor_set_layout.use_bindless = false;
    chip8_image_descriptor_set_layout = _ctx->create_descriptor_set_layout(
        config_chip8_image_descriptor_set_layout);

    gfx::config_sampler_t config_chip8_sampler{};
    config_chip8_sampler.vk_mag_filter = VK_FILTER_NEAREST;
    config_chip8_sampler.vk_min_filter = VK_FILTER_NEAREST;
    chip8_sampler = _ctx->create_sampler(config_chip8_sampler);

    gfx::config_image_t config_chip8_image{};
    config_chip8_image.vk_width = 64;
    config_chip8_image.vk_height = 32;
    config_chip8_image.vk_depth = 1;
    config_chip8_image.vk_type = VK_IMAGE_TYPE_2D;
    config_chip8_image.vk_format = VK_FORMAT_R8G8B8A8_UNORM;
    config_chip8_image.vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    config_chip8_image.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    config_chip8_image.vk_tiling = VK_IMAGE_TILING_LINEAR;
    chip8_image[0] = _ctx->create_image(config_chip8_image);
    chip8_image[1] = _ctx->create_image(config_chip8_image);

    chip8_image_view[0] =
        _ctx->create_image_view({.handle_image = chip8_image[0]});
    chip8_image_view[1] =
        _ctx->create_image_view({.handle_image = chip8_image[1]});

    chip8_descriptor_set[0] = _ctx->allocate_descriptor_set(
        {.handle_descriptor_set_layout = chip8_image_descriptor_set_layout});
    chip8_descriptor_set[1] = _ctx->allocate_descriptor_set(
        {.handle_descriptor_set_layout = chip8_image_descriptor_set_layout});

    _ctx->update_descriptor_set(chip8_descriptor_set[0])
        .push_image_write(0, {.handle_sampler = chip8_sampler,
                              .handle_image_view = chip8_image_view[0],
                              .vk_image_layout = VK_IMAGE_LAYOUT_GENERAL})
        .commit();
    _ctx->update_descriptor_set(chip8_descriptor_set[1])
        .push_image_write(0, {.handle_sampler = chip8_sampler,
                              .handle_image_view = chip8_image_view[1],
                              .vk_image_layout = VK_IMAGE_LAYOUT_GENERAL})
        .commit();

    // transition chip8 images to general
    auto cbuf = gfx::helper::begin_single_use_commandbuffer(
        *_ctx, _base->_command_pool);
    for (auto img : chip8_image) {
      gfx::helper::cmd_transition_image_layout(
          *_ctx, cbuf, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    gfx::helper::end_single_use_command_buffer(*_ctx, cbuf);

    memory_editor.Cols = 8;
  }

  ~renderer_t() {
    // TODO: destroy things
    _ctx->wait_idle();
    gfx::helper::imgui_shutdown();
  }

  void render(chip8_t<64, 32> &chip8) {
    auto [width, height] = _window.dimensions();

    std::memcpy(_ctx->map_image(chip8_image[_base->current_frame()]), chip8.fb,
                sizeof(chip8.fb));
    _ctx->flush_image(chip8_image[_base->current_frame()]);

    _base->begin();
    auto cbuf = _base->current_commandbuffer();

    _ctx->cmd_image_memory_barrier(
        cbuf, _final_image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    gfx::rendering_attachment_t color_rendering_attachment{};
    color_rendering_attachment.clear_value = {0, 0, 0, 0};
    color_rendering_attachment.image_layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
    color_rendering_attachment.handle_image_view = _final_image_view;

    _ctx->cmd_begin_rendering(cbuf, {color_rendering_attachment}, std::nullopt,
                              VkRect2D{VkOffset2D{},
                                       {static_cast<uint32_t>(width),
                                        static_cast<uint32_t>(height)}});

    gfx::helper::imgui_newframe();

    ImGuiDockNodeFlags dockspaceFlags =
        ImGuiDockNodeFlags_None & ~ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoDecoration;

    bool dockSpace = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    auto mainViewPort = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewPort->WorkPos);
    ImGui::SetNextWindowSize(mainViewPort->WorkSize);
    ImGui::SetNextWindowViewport(mainViewPort->ID);

    ImGui::Begin("DockSpace", &dockSpace, windowFlags);
    ImGuiID dockspaceID = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspaceID, ImGui::GetContentRegionAvail(),
                     dockspaceFlags);
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("options")) {
        // for (auto &panel : pipelines) {
        //   if (ImGui::MenuItem(panel->m_name.c_str(), NULL, &panel->show)) {
        //   }
        // }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGuiWindowFlags viewPortFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
    ImGui::Begin("viewport", nullptr, viewPortFlags);
    ImGui::Image(reinterpret_cast<ImTextureID>(reinterpret_cast<void *>(
                     _ctx->get_descriptor_set(
                             chip8_descriptor_set[_base->current_frame()])
                         .vk_descriptor_set)),
                 ImGui::GetContentRegionAvail(), ImVec2(0, -1), ImVec2(1, 0));
    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::Begin("state");
    // stuff
    ImGui::Text("program counter: %04X", chip8.pc);
    ImGui::Text("index: %04X", chip8.I);
    ImGui::Text("V0: %02X", chip8.V0);
    ImGui::SameLine();
    ImGui::Text("V1: %02X", chip8.V1);
    ImGui::SameLine();
    ImGui::Text("V2: %02X", chip8.V2);
    ImGui::SameLine();
    ImGui::Text("V3: %02X", chip8.V3);
    ImGui::Text("V4: %02X", chip8.V4);
    ImGui::SameLine();
    ImGui::Text("V5: %02X", chip8.V5);
    ImGui::SameLine();
    ImGui::Text("V6: %02X", chip8.V6);
    ImGui::SameLine();
    ImGui::Text("V7: %02X", chip8.V7);
    ImGui::Text("V8: %02X", chip8.V8);
    ImGui::SameLine();
    ImGui::Text("V9: %02X", chip8.V9);
    ImGui::SameLine();
    ImGui::Text("VA: %02X", chip8.VA);
    ImGui::SameLine();
    ImGui::Text("VB: %02X", chip8.VB);
    ImGui::Text("VC: %02X", chip8.VC);
    ImGui::SameLine();
    ImGui::Text("VD: %02X", chip8.VD);
    ImGui::SameLine();
    ImGui::Text("VE: %02X", chip8.VE);
    ImGui::SameLine();
    ImGui::Text("VF: %02X", chip8.VF);
    ImGui::Text("stack: ");
    ImGui::SameLine();
    if (chip8.stack.size()) {
      ImGui::Text("%04X", chip8.stack.top());
    } else {
      ImGui::Text("null");
    }
    ImGui::Text("Delay: %02X", chip8.delay);
    ImGui::Text("Sound: %02X", chip8.sound);
    ImGui::Text("key[0]: %d", chip8.keys[0]);
    ImGui::SameLine();
    ImGui::Text("key[1]: %d", chip8.keys[1]);
    ImGui::SameLine();
    ImGui::Text("key[2]: %d", chip8.keys[2]);
    ImGui::SameLine();
    ImGui::Text("key[3]: %d", chip8.keys[3]);
    ImGui::Text("key[4]: %d", chip8.keys[4]);
    ImGui::SameLine();
    ImGui::Text("key[5]: %d", chip8.keys[5]);
    ImGui::SameLine();
    ImGui::Text("key[6]: %d", chip8.keys[6]);
    ImGui::SameLine();
    ImGui::Text("key[7]: %d", chip8.keys[7]);
    ImGui::Text("key[8]: %d", chip8.keys[8]);
    ImGui::SameLine();
    ImGui::Text("key[9]: %d", chip8.keys[9]);
    ImGui::SameLine();
    ImGui::Text("key[A]: %d", chip8.keys[A]);
    ImGui::SameLine();
    ImGui::Text("key[B]: %d", chip8.keys[B]);
    ImGui::Text("key[C]: %d", chip8.keys[C]);
    ImGui::SameLine();
    ImGui::Text("key[D]: %d", chip8.keys[D]);
    ImGui::SameLine();
    ImGui::Text("key[E]: %d", chip8.keys[E]);
    ImGui::SameLine();
    ImGui::Text("key[F]: %d", chip8.keys[F]);
    ImGui::Checkbox("step", &is_step);
    ImGui::End();
    memory_editor.DrawWindow("memory", chip8.memory, sizeof(chip8.memory));

    ImGui::Begin("disassembly");
    for (uint16_t i = chip8.pc - 18; i <= chip8.pc + 18; i+=2) {
      instruction_t inst;
      uint8_t hi = chip8.memory[i];
      uint8_t lo = chip8.memory[i + 1];
      inst._inst = (hi << 8) | lo;

      auto [dis, actual, comment] = chip8.disassembly(inst);
      if (i == chip8.pc) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              IM_COL32(255, 255, 0, 255)); // Yellow highlight
      }

      ImGui::Text("%04X %s %s   %s", i, dis.c_str(), actual.c_str(), comment.c_str());

      if (i == chip8.pc) {
        ImGui::PopStyleColor();
      }
    }
    ImGui::End();

    gfx::helper::imgui_endframe(*_ctx, cbuf);

    _ctx->cmd_end_rendering(cbuf);

    // transition image to shader read only optimal
    _ctx->cmd_image_memory_barrier(
        cbuf, _final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    _base->end();
  }

  core::window_t &_window;
  const bool _is_validating;

  core::ref<gfx::context_t> _ctx;

  gfx::handle_sampler_t _swapchain_sampler;
  gfx::handle_image_t _final_image;
  gfx::handle_image_view_t _final_image_view;

  core::ref<gfx::base_t> _base;

  gfx::handle_descriptor_set_layout_t chip8_image_descriptor_set_layout;
  gfx::handle_sampler_t chip8_sampler;

  gfx::handle_image_t chip8_image[2];
  gfx::handle_image_view_t chip8_image_view[2];
  gfx::handle_descriptor_set_t chip8_descriptor_set[2];

  MemoryEditor memory_editor{};

  bool is_step;
};

#endif
