#include "app.hpp"

#include "core/components.hpp"
#include "core/model.hpp"

#include "renderer.hpp"
#include "component_types.hpp"
#include "utility.hpp"

app_t::app_t() {
    window = core::make_ref<core::window_t>("Application", 1200, 800);
    context = core::make_ref<gfx::context_t>(true);
    auto [width, height] = window->dimensions();
    std::tie(final_image, final_image_view) = gfx::helper::create_2D_image_and_image_view(*context, width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "final_image");
    gloabl_sampler = context->create_sampler({});
    base_renderer = core::make_ref<renderer::base_renderer_t>(*window, *context, gloabl_sampler, final_image_view);
    gfx::helper::imgui_init(*window, *base_renderer, VK_FORMAT_R8G8B8A8_UNORM);
}

app_t::~app_t() {
    context->wait_idle();
    gfx::helper::imgui_shutdown();
}

void app_t::run() {
    core::frame_timer_t frame_timer{ 60.f };
    auto [width, height] = window->dimensions();


    debug_panel_t debug_panel{};
    viewport_panel_t viewport_panel{ dispatcher, *context };
    
    core::scene_t scene{};
    {
        auto id = scene.create();
        scene.construct<core::model_t>(id) = core::load_model_from_path("../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
        scene.construct<core::transform_t>(id) = {};
    }

    core::camera_t camera{};
    camera_controller_t controller{ *window };

    renderer_t renderer{ *base_renderer, gloabl_sampler, width, height };
    renderer.prepare(scene);

    viewport_panel.set_image(renderer.rendered_image_view(), gloabl_sampler);

    while (!window->should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();
        if (glfwGetKey(*window, GLFW_KEY_ESCAPE)) break;
        if (glfwGetKey(*window, GLFW_KEY_R)) {
            renderer.reload();
        }
        auto dt = frame_timer.update();

        controller.update(dt.count(), camera);

        // maybe begin should return a cbuff ?
        base_renderer->begin();
        auto cbuff = base_renderer->current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        {
            renderer.render(cbuff, scene, camera, width, height);

            context->cmd_image_memory_barrier(cbuff, final_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            gfx::rendering_attachment_t attachment{};
            attachment.handle_image_view = final_image_view;

            context->cmd_begin_rendering(cbuff, { attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });

            gfx::helper::imgui_newframe();
            
            bool dockSpace = true;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            auto mainViewPort = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(mainViewPort->WorkPos);
            ImGui::SetNextWindowSize(mainViewPort->WorkSize);
            ImGui::SetNextWindowViewport(mainViewPort->ID);

            ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None & ~ImGuiDockNodeFlags_PassthruCentralNode;
            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking             | 
                                           ImGuiWindowFlags_NoTitleBar            |
                                           ImGuiWindowFlags_NoCollapse            |
                                           ImGuiWindowFlags_NoResize              |
                                           ImGuiWindowFlags_NoMove                |
                                           ImGuiWindowFlags_NoBringToFrontOnFocus |
                                           ImGuiWindowFlags_NoNavFocus            |
                                           ImGuiWindowFlags_NoBackground          |
                                           ImGuiWindowFlags_NoDecoration;
                                        
            ImGui::Begin("DockSpace", &dockSpace, windowFlags);
            ImGuiID dockspaceID = ImGui::GetID("DockSpace");
            ImGui::DockSpace(dockspaceID, ImGui::GetContentRegionAvail(), dockspaceFlags);
            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("Panels")) {
                    if (ImGui::MenuItem(debug_panel.name.c_str(), nullptr, &debug_panel.show)) {}
                    if (ImGui::MenuItem(viewport_panel.name.c_str(), nullptr, &viewport_panel.show)) {}
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
            
            viewport_panel.render();
            debug_panel.render();
            
            gfx::helper::imgui_endframe(*base_renderer, cbuff);

            context->cmd_end_rendering(cbuff);

            context->cmd_image_memory_barrier(cbuff, final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        }

        base_renderer->end();

    }
}
