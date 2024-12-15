#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/context.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/helper.hpp"
#include "imgui.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

int main() {
// 	core::log_t::set_log_level(core::log_level_t::e_info);

	core::window_t window{ "app", 640, 420 };
	auto [width, height] = window.dimensions();

	gfx::context_t context{ true };

	gfx::handle_sampler_t sampler = context.create_sampler({});

	VkFormat final_image_format = VK_FORMAT_R8G8B8A8_SRGB;
	gfx::config_image_t config_final_image{};
	config_final_image.vk_width		= width;
	config_final_image.vk_height	= height;
	config_final_image.vk_depth		= 1;
	config_final_image.vk_type		= VK_IMAGE_TYPE_2D;
	config_final_image.vk_format	= final_image_format;
	config_final_image.vk_usage		= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	config_final_image.vk_mips		= 1;
	gfx::handle_image_t final_image = context.create_image(config_final_image);
	gfx::handle_image_view_t final_image_view = context.create_image_view({ .handle_image = final_image });

	gfx::base_config_t base_config{
		.window		= window,
		.context	= context,
		.sampler	= sampler,
		.final_image_view = final_image_view
	};
	gfx::base_t base{ base_config };

	gfx::helper::imgui_init(window, context, base._swapchain, final_image_format);

	while (!window.should_close()) {
		core::clear_frame_function_times();
		core::window_t::poll_events();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE)) break;

		base.begin();
		auto cbuf = base.current_commandbuffer();

		// Transition image to color attachment optimal
		context.cmd_image_memory_barrier(cbuf, final_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		gfx::rendering_attachment_t color_rendering_attachment{};
		color_rendering_attachment.clear_value = { 0, 0, 0, 0 };
		color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
		color_rendering_attachment.handle_image_view = final_image_view;

		// begin rendering
		context.cmd_begin_rendering(cbuf, { color_rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });

		gfx::helper::imgui_newframe();
		ImGui::Begin("test");
		ImGui::End();
		gfx::helper::imgui_endframe(context, cbuf);

		// end rendering
		context.cmd_end_rendering(cbuf);

		// transition image to shader read only optimal
		context.cmd_image_memory_barrier(cbuf, final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		base.end();
	}
	context.wait_idle();

	gfx::helper::imgui_shutdown();

	return 0;
}
