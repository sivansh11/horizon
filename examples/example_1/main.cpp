#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"
#include "horizon/core/model.hpp"

#include "horizon/gfx/context.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/helper.hpp"

#include <GLFW/glfw3.h>
#include <cstring>
#include <vulkan/vulkan_core.h>


const std::vector<core::vertex_t> vertices {
	core::vertex_t{ {-1 / 2.f, -1 / 2.f, 0}, {}, {0, 0} },
	core::vertex_t{ {-1 / 2.f,  1 / 2.f, 0}, {}, {0, 1} },
	core::vertex_t{ { 1 / 2.f,  1 / 2.f, 0}, {}, {1, 1} },
	core::vertex_t{ {-1 / 2.f, -1 / 2.f, 0}, {}, {0, 0} },
	core::vertex_t{ { 1 / 2.f,  1 / 2.f, 0}, {}, {1, 1} },
	core::vertex_t{ { 1 / 2.f, -1 / 2.f, 0}, {}, {1, 0} },
};

const std::vector<uint32_t> indices {
	0, 1, 2,
	3, 4, 5,
};

int main() {
	core::ref<core::window_t> win = core::make_ref<core::window_t>("test", 640, 420);
	auto [width, height] = win->dimensions();
	core::ref<gfx::context_t> ctx = core::make_ref<gfx::context_t>(true);

	gfx::handle_sampler_t sampler = ctx->create_sampler({});
	gfx::config_image_t config_final_image{};
	config_final_image.vk_width		= width;
	config_final_image.vk_height 	= height;
	config_final_image.vk_depth  	= 1;
	config_final_image.vk_type		= VK_IMAGE_TYPE_2D;
	config_final_image.vk_format	= VK_FORMAT_R8G8B8A8_SRGB;
	config_final_image.vk_usage		= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	config_final_image.vk_mips		= 1;
	gfx::handle_image_t final_image = ctx->create_image(config_final_image);
	gfx::handle_image_view_t final_image_view = ctx->create_image_view({ .handle_image = final_image });

	gfx::base_info_t base_info{
		.window					= *win.get(),
		.context				= *ctx.get(),
		.sampler				= sampler,
		.final_image_view		= final_image_view,
	};

	gfx::base_t base{ base_info };

	gfx::config_descriptor_set_layout_t cdsl{};
	cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
		.add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
	gfx::handle_descriptor_set_layout_t dsl = ctx->create_descriptor_set_layout(cdsl);

	gfx::config_pipeline_layout_t cpl{};
	cpl.add_descriptor_set_layout(dsl);
	gfx::handle_pipeline_layout_t pl = ctx->create_pipeline_layout(cpl);

	gfx::config_pipeline_t cp{};
	cp.handle_pipeline_layout = pl;
	cp.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());			// THIS should be in helper ?
	cp.add_shader(
		ctx->create_shader({
			.code_or_path = "../../assets/shaders/example_1/vert.slang",
			.is_code = false,
			.name = "vertex shader",
			.type = gfx::shader_type_t::e_vertex,
			.language = gfx::shader_language_t::e_slang
		})
	);
	cp.add_shader(
		ctx->create_shader({
			.code_or_path = "../../assets/shaders/example_1/frag.slang",
			.is_code = false,
			.name = "fragment shader",
			.type = gfx::shader_type_t::e_fragment,
			.language = gfx::shader_language_t::e_slang
		})
	);
	gfx::handle_pipeline_t p = ctx->create_graphics_pipeline(cp);

	gfx::config_buffer_t cib{};
	cib.vk_size = indices.size() * sizeof(indices[0]);
	cib.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	cib.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	gfx::handle_buffer_t ib = ctx->create_buffer(cib);
	std::memcpy(ctx->map_buffer(ib), indices.data(), indices.size() * sizeof(indices[0]));

	gfx::config_buffer_t cvb{};
	cvb.vk_size = vertices.size() * sizeof(vertices[0]);
	cvb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	cvb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	gfx::handle_buffer_t vb = ctx->create_buffer(cvb);
	std::memcpy(ctx->map_buffer(vb), vertices.data(), vertices.size() * sizeof(vertices[0]));

	gfx::handle_descriptor_set_t ds = ctx->allocate_descriptor_set({ .handle_descriptor_set_layout = dsl });
	ctx->update_descriptor_set(ds)
		.push_buffer_write(0, { .handle_buffer = ib })
		.push_buffer_write(1, { .handle_buffer = vb })
		.commit();

	while (!win->should_close()) {
		core::clear_frame_function_times();
		core::window_t::poll_events();
		if (glfwGetKey(*win, GLFW_KEY_ESCAPE)) break;

		base.begin();

		auto cbuf = base.current_commandbuffer();
		auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

		gfx::rendering_attachment_t color_rendering_attachment{};
		color_rendering_attachment.clear_value = { 0, 0, 0, 0 };
		color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
		color_rendering_attachment.handle_image_view = final_image_view;
		
		ctx->cmd_image_memory_barrier(cbuf, final_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		
		ctx->cmd_begin_rendering(cbuf, { color_rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
		ctx->cmd_bind_pipeline(cbuf, p);
		ctx->cmd_bind_descriptor_sets(cbuf, p, 0, { ds });
		ctx->cmd_set_viewport_and_scissor(cbuf, viewport, scissor);
		ctx->cmd_draw(cbuf, 6, 1, 0, 0);
		ctx->cmd_end_rendering(cbuf);

		ctx->cmd_image_memory_barrier(cbuf, final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

		base.end();
	}

	return 0;
}
