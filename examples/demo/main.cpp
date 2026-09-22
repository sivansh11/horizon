#include <algorithm>
#include <cstring>

#include "model/model.hpp"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include "camera.hpp"
#include "horizon/core/components.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"

struct mesh_t {
  gfx::handle_buffer_t vertices;
  gfx::handle_buffer_t indices;

  uint32_t vertex_count;
  uint32_t index_count;

  gfx::handle_bindless_image_t diffuse;
};

struct viewport_t {
  gfx::handle_image_t      image;
  gfx::handle_image_view_t image_view;
  gfx::handle_image_t      depth;
  gfx::handle_image_view_t depth_view;
};

viewport_t create_viewport(core::ref<gfx::base_t> base,   //
                           uint32_t               width,  //
                           uint32_t               height) {
  gfx::config_image_t ci{};
  ci.vk_width                    = width;
  ci.vk_height                   = height;
  ci.vk_depth                    = 1;
  ci.vk_type                     = VK_IMAGE_TYPE_2D;
  ci.vk_format                   = VK_FORMAT_R8G8B8A8_UNORM;
  ci.vk_usage                    = VK_IMAGE_USAGE_SAMPLED_BIT |  //
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  ci.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  ci.debug_name                  = "image";
  // TODO: create base->create_image
  gfx::handle_image_t image = base->_context->create_image(ci);
  // TODO: create base->create_image_view
  gfx::handle_image_view_t image_view = base->_context->create_image_view(
      {.handle_image = image, .debug_name = "image_view"});

  ci.vk_format  = VK_FORMAT_D32_SFLOAT;
  ci.vk_usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  ci.debug_name = "depth";
  // TODO: create base->create_image
  gfx::handle_image_t depth = base->_context->create_image(ci);
  // TODO: create base->create_image_view
  gfx::handle_image_view_t depth_view = base->_context->create_image_view(
      {.handle_image = depth, .debug_name = "depth_view"});

  return {image, image_view, depth, depth_view};
}

void destroy_viewport(core::ref<gfx::base_t> base, const viewport_t &viewport) {
  // TODO: add base->wait_idle
  base->_context->wait_idle();
  // TODO: add base->destroy_image
  base->_context->destroy_image(viewport.image);
  // TODO: add base->destroy_image_view
  base->_context->destroy_image_view(viewport.image_view);
  // TODO: add base->destroy_image
  base->_context->destroy_image(viewport.depth);
  // TODO: add base->destroy_image_view
  base->_context->destroy_image_view(viewport.depth_view);
}

gfx::handle_image_t create_image_from_color(core::ref<gfx::base_t> base,
                                            math::vec4             color) {
  gfx::config_buffer_t cb{};
  cb.vk_size               = 4;
  cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  cb.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  // TODO: start using base create_buffer
  gfx::handle_buffer_t staging = base->_context->create_buffer(cb);
  uint32_t             pixel   = uint8_t(color.x * 255.f) |        //
                                 uint8_t(color.y * 255.f) << 8 |   //
                                 uint8_t(color.z * 255.f) << 16 |  //
                                 uint8_t(color.w * 255.f) << 24;
  // TODO: use base->map_buffer
  std::memcpy(base->_context->map_buffer(staging), &pixel, sizeof(pixel));

  gfx::config_image_t ci{};
  ci.vk_width  = 1;
  ci.vk_height = 1;
  ci.vk_depth  = 1;
  ci.vk_type   = VK_IMAGE_TYPE_2D;
  ci.vk_format = VK_FORMAT_R8G8B8A8_UNORM;
  ci.vk_usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  ci.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  ci.debug_name                  = "default";
  // TODO: add base->create_image
  gfx::handle_image_t image = base->_context->create_image(ci);

  // TODO: add begin_single_use_commandbuffer variant using base
  gfx::handle_commandbuffer_t cmd = gfx::helper::begin_single_use_commandbuffer(
      *base->_context, base->_command_pool);
  // TODO: add base variant of cmd_transition_image_layout
  gfx::helper::cmd_transition_image_layout(
      *base->_context,            //
      cmd,                        //
      image,                      //
      VK_IMAGE_LAYOUT_UNDEFINED,  //
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  // TODO: add base->cmd_copy_buffer_to_image
  // TODO: add custom VkBufferImageCopy with defaults
  base->_context->cmd_copy_buffer_to_image(
      cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      {
          .imageSubresource =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .layerCount = 1,
              },
          .imageExtent = {1, 1, 1},
      });
  // TODO: add base variant of cmd_transition_image_layout
  gfx::helper::cmd_transition_image_layout(
      *base->_context,                       //
      cmd,                                   //
      image,                                 //
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  //
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  // TODO: add end_single_use_command_buffer variant using base
  gfx::helper::end_single_use_command_buffer(*base->_context, cmd);
  return image;
}

struct push_constant_t {
  core::camera_t                *camera;
  model::vertex_t               *vertices;
  uint32_t                      *indices;
  gfx::handle_bindless_image_t   b_diffuse;
  gfx::handle_bindless_sampler_t b_sampler;
};
static_assert(sizeof(push_constant_t) <= 128,
              "sizeof(push_constant_t) not <= 128");

int main(int argc, char **argv) {
  core::ref<core::window_t> window =
      core::make_ref<core::window_t>("demo", 640, 420);
  core::ref<gfx::context_t> context = core::make_ref<gfx::context_t>(true);
  core::ref<gfx::base_t>    base = core::make_ref<gfx::base_t>(window, context);

  gfx::helper::imgui_init(
      *window,           //
      *context,          //
      base->_swapchain,  //
      context
          ->get_image(context->get_swapchain(base->_swapchain).handle_images[0])
          .config.vk_format);

  // TODO: add base->create_sampler
  gfx::handle_sampler_t          sampler   = base->_context->create_sampler({});
  gfx::handle_bindless_sampler_t b_sampler = base->new_bindless_sampler();
  base->set_bindless_sampler(b_sampler, sampler);

  gfx::config_descriptor_set_layout_t cdsl{};
  cdsl.add_layout_binding(0,  //
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          VK_SHADER_STAGE_FRAGMENT_BIT);
  cdsl.use_bindless = false;
  // TODO: add base->create_descriptor_set_layout
  gfx::handle_descriptor_set_layout_t imgui_dsl =
      base->_context->create_descriptor_set_layout(cdsl);

  // TODO: create a helper to allocate imgui's ds
  // TODO: switch to using base's allocate_descriptor_set
  gfx::handle_descriptor_set_t imgui_ds =
      base->_context->allocate_descriptor_set(
          {.handle_descriptor_set_layout = imgui_dsl});

  uint32_t   image_width  = 5;
  uint32_t   image_height = 5;
  viewport_t viewport     = create_viewport(base, image_width, image_height);

  // TODO: add a helper to update_descriptor_set for imgui
  context->update_descriptor_set(imgui_ds)
      .push_image_write(
          0, {.handle_sampler    = sampler,
              .handle_image_view = viewport.image_view,
              .vk_image_layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
      .commit();

  // TODO: add if check argc
  model::raw_model_t raw_model = model::load_model_from_path(argv[1]);

  std::vector<mesh_t> meshes;
  for (auto &raw_mesh : raw_model.meshes) {
    mesh_t &mesh      = meshes.emplace_back();
    mesh.vertex_count = raw_mesh.vertices.size();
    mesh.index_count  = raw_mesh.indices.size();

    gfx::config_buffer_t cb{};
    cb.vk_size = raw_mesh.vertices.size() * sizeof(raw_mesh.vertices[0]);
    cb.vk_buffer_usage_flags       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    cb.debug_name                  = "vertex buffer";
    // TODO: add helper create_buffer_staged using base instead of context
    mesh.vertices =
        gfx::helper::create_buffer_staged(*base->_context,           //
                                          base->_command_pool,       //
                                          cb,                        //
                                          raw_mesh.vertices.data(),  //
                                          cb.vk_size);
    cb.vk_size    = raw_mesh.indices.size() * sizeof(raw_mesh.indices[0]);
    cb.debug_name = "index buffer";
    // TODO: add helper create_buffer_staged using base instead of context
    mesh.indices =
        gfx::helper::create_buffer_staged(*base->_context,          //
                                          base->_command_pool,      //
                                          cb,                       //
                                          raw_mesh.indices.data(),  //
                                          cb.vk_size);

    auto it = std::find_if(raw_mesh.material_description.texture_infos.begin(),
                           raw_mesh.material_description.texture_infos.end(),
                           [](model::texture_info_t &info) {
                             return info.texture_type ==
                                    model::texture_type_t::e_diffuse_map;
                           });
    // TODO: add helper in model to get optional if texture type == type
    if (it != raw_mesh.material_description.texture_infos.end()) {
      // TODO: add base version of helper load_image_from_path_instant
      gfx::handle_image_t image =
          gfx::helper::load_image_from_path_instant(*base->_context,      //
                                                    base->_command_pool,  //
                                                    it->file_path,        //
                                                    VK_FORMAT_R8G8B8A8_SRGB);
      // TODO: add base->create_image_view
      gfx::handle_image_view_t image_view =
          base->_context->create_image_view({.handle_image = image});
      gfx::handle_bindless_image_t bindless_image = base->new_bindless_image();
      base->set_bindless_image(bindless_image,  //
                               image_view,      //
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      mesh.diffuse = bindless_image.val;
    } else {
      auto it = std::find_if(
          raw_mesh.material_description.texture_infos.begin(),
          raw_mesh.material_description.texture_infos.end(),
          [](model::texture_info_t &info) {
            return info.texture_type == model::texture_type_t::e_diffuse_color;
          });

      check(it != raw_mesh.material_description.texture_infos.end(),
            "material doesnt have diffuse map or diffuse color");
      gfx::handle_image_t image =
          create_image_from_color(base, it->diffuse_color);
      // TODO: add base->create_image_view
      gfx::handle_image_view_t image_view =
          base->_context->create_image_view({.handle_image = image});
      gfx::handle_bindless_image_t bindless_image = base->new_bindless_image();
      base->set_bindless_image(bindless_image,  //
                               image_view,      //
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      mesh.diffuse = bindless_image.val;
    }
  }

  gfx::config_pipeline_layout_t cpl{};
  cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
  cpl.add_descriptor_set_layout(base->_bindless_descriptor_set_layout);
  // TODO: add base->create_pipeline_layout
  // TODO: base should have a helper that returns a precreated bindless pipeline
  // layout
  gfx::handle_pipeline_layout_t pl =
      base->_context->create_pipeline_layout(cpl);
  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  // TODO: add helper to get format from image
  cp.add_color_attachment(
      base->_context->get_image(viewport.image).config.vk_format,
      gfx::default_color_blend_attachment());
  cp.set_depth_attachment(
      base->_context->get_image(viewport.depth).config.vk_format,
      VkPipelineDepthStencilStateCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable   = VK_TRUE,
          .depthWriteEnable  = VK_TRUE,
          .depthCompareOp    = VK_COMPARE_OP_LESS,
          .stencilTestEnable = VK_FALSE,
      });
  // TODO: add helper variant with base
  cp.add_shader(gfx::helper::create_slang_shader(  //
      *base->_context, "./examples/demo/shaders/diffuse.slang",
      gfx::shader_type_t::e_vertex));
  // TODO: add helper variant with base
  cp.add_shader(gfx::helper::create_slang_shader(
      *base->_context, "./examples/demo/shaders/diffuse.slang",
      gfx::shader_type_t::e_fragment));
  // TODO: add base->create_graphics_pipeline
  gfx::handle_pipeline_t p = base->_context->create_graphics_pipeline(cp);

  gfx::config_buffer_t cb{};
  cb.vk_size               = sizeof(core::camera_t);
  cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  cb.debug_name = "camera";
  gfx::handle_managed_buffer_t camera_buffer =
      base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);

  bool                resize_image = false;
  core::frame_timer_t frame_timer{60.f};
  editor_camera_t     camera{*window};

  horizon_info("entering main while loop");
  while (!window->should_close()) {
    core::window_t::poll_events();
    if (window->get_key_pressed(core::key_t::e_escape)) break;
    if (window->get_key_pressed(core::key_t::e_q)) break;

    core::timer::duration_t dt = frame_timer.update();
    // TODO: if should update, set mouse position to center of screen
    camera.update(dt.count(), image_width, image_height);
    std::memcpy(base->map_buffer(camera_buffer),         //
                &static_cast<core::camera_t &>(camera),  //
                sizeof(core::camera_t));

    base->begin();

    gfx::rendergraph_t rendergraph{};
    rendergraph
        .add_pass([&](gfx::handle_commandbuffer_t cmd) {
          //
          gfx::rendering_attachment_t image_attachment{};
          image_attachment.handle_image_view = viewport.image_view;

          gfx::rendering_attachment_t depth_attachment{};
          depth_attachment.handle_image_view = viewport.depth_view;
          depth_attachment.image_layout =
              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
          depth_attachment.clear_value.depthStencil.depth = 1;
          // TODO: add base->cmd_begin_rendering
          base->_context->cmd_begin_rendering(cmd,                 //
                                              {image_attachment},  //
                                              depth_attachment,    //
                                              {{},
                                               {image_width,  //
                                                image_height}});
          auto [vk_viewport, vk_scissor] =
              gfx::helper::fill_viewport_and_scissor_structs(image_width,
                                                             image_height);
          // TODO: add base->cmd_bind_pipeline
          base->_context->cmd_bind_pipeline(cmd, p);
          // TODO: add base->cmd_bind_descriptor_sets
          // TODO: add a helper to bind _bindless_descriptor_set
          base->_context->cmd_bind_descriptor_sets(
              cmd, p, 0, {base->_bindless_descriptor_set});
          // TODO: add base->cmd_set_viewport_and_scissor
          base->_context->cmd_set_viewport_and_scissor(cmd,          //
                                                       vk_viewport,  //
                                                       vk_scissor);
          for (auto &mesh : meshes) {
            push_constant_t pc{};
            pc.camera = gfx::to<core::camera_t *>(
                base->get_buffer_device_address(camera_buffer));
            // TODO: add base->get_buffer_device_address overload for normal
            // buffers
            pc.vertices = gfx::to<model::vertex_t *>(
                base->_context->get_buffer_device_address(mesh.vertices));
            // TODO: add base->get_buffer_device_address overload for normal
            // buffers
            pc.indices = gfx::to<uint32_t *>(
                base->_context->get_buffer_device_address(mesh.indices));
            pc.b_diffuse = mesh.diffuse;
            pc.b_sampler = b_sampler;
            // TODO: add base->cmd_push_constants
            base->_context->cmd_push_constants(cmd,                      //
                                               p,                        //
                                               VK_SHADER_STAGE_ALL,      //
                                               0,                        //
                                               sizeof(push_constant_t),  //
                                               &pc);
            base->cmd_draw(cmd, mesh.index_count, 1, 0, 0);
          }
          base->cmd_end_rendering(cmd);
        })
        .add_write_image(viewport.image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .add_write_image(viewport.depth,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    rendergraph
        .add_pass([&](gfx::handle_commandbuffer_t cmd) {
          auto [width, height] = window->dimensions();
          VkRect2D render_area{{}, {(uint32_t)width, (uint32_t)height}};
          auto     attachment = base->swapchain_rendering_attachment(
              {0, 0, 0, 0},                              //
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  //
              VK_ATTACHMENT_LOAD_OP_CLEAR,               //
              VK_ATTACHMENT_STORE_OP_STORE);
          base->cmd_begin_rendering(cmd,           //
                                    {attachment},  //
                                    std::nullopt,  //
                                    render_area);

          gfx::helper::imgui_newframe();
          ImGuiDockNodeFlags dockspaceFlags =
              ImGuiDockNodeFlags_None & ~ImGuiDockNodeFlags_PassthruCentralNode;
          ImGuiWindowFlags windowFlags =
              ImGuiWindowFlags_NoDocking |              //
              ImGuiWindowFlags_NoTitleBar |             //
              ImGuiWindowFlags_NoCollapse |             //
              ImGuiWindowFlags_NoResize |               //
              ImGuiWindowFlags_NoMove |                 //
              ImGuiWindowFlags_NoBringToFrontOnFocus |  //
              ImGuiWindowFlags_NoNavFocus |             //
              ImGuiWindowFlags_NoBackground |           //
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
          static bool settings = true;

          if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("menu")) {
              if (ImGui::MenuItem("show settings", NULL, &settings)) {
              }
              ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
          }
          ImGui::End();
          ImGui::PopStyleVar(2);

          ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
          ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
          ImGuiWindowClass window_class;
          window_class.DockNodeFlagsOverrideSet =
              ImGuiDockNodeFlags_AutoHideTabBar;
          ImGui::SetNextWindowClass(&window_class);
          ImGuiWindowFlags viewPortFlags =
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
          ImGui::Begin("viewport", nullptr, viewPortFlags);

          // ImVec2 mouse_position  = ImGui::GetMousePos();
          // ImVec2 window_position = ImGui::GetWindowPos();
          auto vp = ImGui::GetWindowSize();
          if (image_width != vp.x || image_height != vp.y) {
            image_width  = vp.x;
            image_height = vp.y;
            resize_image = true;
          }
          ImGui::Image(
              reinterpret_cast<ImTextureID>(reinterpret_cast<void *>(
                  context->get_descriptor_set(imgui_ds).vk_descriptor_set)),
              ImGui::GetContentRegionAvail());
          ImGui::End();
          ImGui::PopStyleVar(2);

          if (settings) {
            ImGui::Begin("settings", &settings);
            ImGui::Text("%f fps", ImGui::GetIO().Framerate);
            ImGui::DragFloat("camera speed", &camera.camera_speed_multiplyer);
            ImGui::End();
          }
          gfx::helper::imgui_endframe(*context, cmd);

          base->cmd_end_rendering(cmd);
        })
        .add_read_image(viewport.image, VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .add_write_image(base->current_swapchain_image(),
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rendergraph.add_pass([&](gfx::handle_commandbuffer_t cmd) {})
        .add_write_image(base->current_swapchain_image(), 0,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    gfx::handle_commandbuffer_t cmd = base->current_commandbuffer();
    base->render_rendergraph(rendergraph, cmd);

    base->end();

    if (resize_image) {
      resize_image = false;
      destroy_viewport(base, viewport);
      viewport = create_viewport(base, image_width, image_height);
      context->update_descriptor_set(imgui_ds)
          .push_image_write(
              0, {.handle_sampler    = sampler,
                  .handle_image_view = viewport.image_view,
                  .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
          .commit();
    }
  }

  // TODO: add base->wait_idle
  base->_context->wait_idle();

  gfx::helper::imgui_shutdown();

  return 0;
}
