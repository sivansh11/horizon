#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "core/math.hpp"

class clear_pass_t {
    struct uniform_t {
        uint32_t width;
        uint32_t height;
        glm::vec4 clear_color;
    };
public:
    clear_pass_t(core::window_t& window, renderer::base_renderer_t& base_renderer) : window(window), base_renderer(base_renderer) {
        auto [width, height] = window.dimensions();

        gfx::config_descriptor_set_layout_t cdsl{};
        cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        dsl = base_renderer.context.create_descriptor_set_layout(cdsl);

        gfx::config_pipeline_layout_t cpl{};
        cpl.add_descriptor_set_layout(dsl);
        pl = base_renderer.context.create_pipeline_layout(cpl);

        gfx::config_shader_t cs{};
        cs.code_or_path = "../../assets/shaders/rtiw2/clear.slang";
        cs.is_code = false;
        cs.name = "clear";
        cs.type = gfx::shader_type_t::e_compute;
        cs.language = gfx::shader_language_t::e_slang;
        s = base_renderer.context.create_shader(cs);

        gfx::config_pipeline_t cp{};
        cp.handle_pipeline_layout = pl;
        cp.add_shader(s);
        p = base_renderer.context.create_compute_pipeline(cp);

        ds = base_renderer.context.allocate_descriptor_set({ .handle_descriptor_set_layout = dsl });

        gfx::config_buffer_t cb{};
        cb.vk_size = sizeof(uniform_t);
        cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        cb.vma_allocation_create_flags =    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        b = base_renderer.context.create_buffer(cb);

        base_renderer.context.update_descriptor_set(ds)
            .push_buffer_write(0, { .handle_buffer = b })
            .commit();

    }

    ~clear_pass_t() {
        base_renderer.context.destroy_descriptor_set_layout(dsl);
        base_renderer.context.destroy_pipeline_layout(pl);
        base_renderer.context.destroy_shader(s);
        base_renderer.context.destroy_pipeline(p);
        base_renderer.context.free_descriptor_set(ds);
    }

    void update_target_image(gfx::handle_sampler_t sampler, gfx::handle_image_t image, gfx::handle_image_view_t image_view) {
        target_sampler = sampler;
        target_image = image;
        target_image_view = image_view;
        base_renderer.context.update_descriptor_set(ds)
            .push_image_write(1, { .handle_sampler = sampler, .handle_image_view = image_view, .vk_image_layout = VK_IMAGE_LAYOUT_GENERAL })
            .commit();
    }

    void update_uniform(uint32_t width, uint32_t height, const glm::vec4 clear_color) {
        target_width = width;
        target_height = height;

        uniform_t *p_uniform = reinterpret_cast<uniform_t *>(base_renderer.context.map_buffer(b));

        p_uniform->width = width;
        p_uniform->height = height;
        p_uniform->clear_color = clear_color;
    }

    void render(gfx::handle_commandbuffer_t commandbuffer) {
        assert(target_width != 0 && target_height != 0 && target_image != gfx::null_handle && target_image_view != gfx::null_handle && target_sampler != gfx::null_handle);

        base_renderer.context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        base_renderer.context.cmd_bind_pipeline(commandbuffer, p);
        base_renderer.context.cmd_bind_descriptor_sets(commandbuffer, p, 0, { ds });
        base_renderer.context.cmd_dispatch(commandbuffer, (target_width / 8) + 1, (target_height / 8) + 1, 1);

        // maybe I dont need this transition ?
        base_renderer.context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

private:
    core::window_t& window;
    renderer::base_renderer_t& base_renderer;

    gfx::handle_descriptor_set_layout_t dsl;
    gfx::handle_pipeline_layout_t pl;
    gfx::handle_shader_t s;
    gfx::handle_pipeline_t p;
    gfx::handle_descriptor_set_t ds;
    gfx::handle_buffer_t b;

    uint32_t target_width = 0, target_height = 0;
    gfx::handle_image_t target_image = gfx::null_handle;
    gfx::handle_image_view_t target_image_view = gfx::null_handle;
    gfx::handle_sampler_t target_sampler = gfx::null_handle;
};

class raytracer_pass_t {
    struct uniform_t {
        glm::mat4        inverse_projection;
        glm::mat4        inverse_view;
        uint32_t         width; 
        uint32_t         height;
        uint32_t         blas_instance_count;
        uint32_t         samples;
        VkDeviceAddress  p_blas_instances;
        VkDeviceAddress  p_materials;
        VkDeviceAddress  p_bvh;
    };
public:
    raytracer_pass_t(core::window_t& window, renderer::base_renderer_t& base_renderer) : window(window), base_renderer(base_renderer) {
        auto [width, height] = window.dimensions();

        gfx::config_descriptor_set_layout_t cdsl{};
        cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        dsl = base_renderer.context.create_descriptor_set_layout(cdsl);

        gfx::config_pipeline_layout_t cpl{};
        cpl.add_descriptor_set_layout(dsl);
        pl = base_renderer.context.create_pipeline_layout(cpl);

        gfx::config_shader_t cs{};
        cs.code_or_path = "../../assets/shaders/rtiw2/raytracing.slang";
        cs.is_code = false;
        cs.name = "raytracer";
        cs.type = gfx::shader_type_t::e_compute;
        cs.language = gfx::shader_language_t::e_slang;
        s = base_renderer.context.create_shader(cs);

        gfx::config_pipeline_t cp{};
        cp.handle_pipeline_layout = pl;
        cp.add_shader(s);
        p = base_renderer.context.create_compute_pipeline(cp);

        ds = base_renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = dsl });

        gfx::config_buffer_t cb{};
        cb.vk_size = sizeof(uniform_t);
        cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        cb.vma_allocation_create_flags =    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        b = base_renderer.create_buffer(renderer::resource_policy_t::e_every_frame, cb);

        base_renderer.update_descriptor_set(ds)
            .push_buffer_write(0, { .handle_buffer = b })
            .commit();

        t = base_renderer.context.create_timer({});

    }

    ~raytracer_pass_t() {
        base_renderer.context.destroy_descriptor_set_layout(dsl);
        base_renderer.context.destroy_pipeline_layout(pl);
        base_renderer.context.destroy_shader(s);
        base_renderer.context.destroy_pipeline(p);
        // base_renderer.free_descriptor_set(ds);
        // base_renderer.destroy_buffer(b);
    }

    void update_target_image(gfx::handle_sampler_t sampler, gfx::handle_image_t image, gfx::handle_image_view_t image_view) {
        target_sampler = sampler;
        target_image = image;
        target_image_view = image_view;
        base_renderer.update_descriptor_set(ds)
            .push_image_write(1, { .handle_sampler = sampler, .handle_image_view = image_view, .vk_image_layout = VK_IMAGE_LAYOUT_GENERAL })
            .commit();
    }

    void update_uniform(glm::mat4 inverse_projection, glm::mat4 inverse_view, uint32_t  width, uint32_t  height, uint32_t  blas_instance_count, uint32_t samples, VkDeviceAddress p_blas_instances, VkDeviceAddress p_materials, VkDeviceAddress p_bvh) {
        target_width = width;
        target_height = height;

        uniform_t *p_uniform = reinterpret_cast<uniform_t *>(base_renderer.context.map_buffer(base_renderer.buffer(b)));
        
        p_uniform->inverse_projection = inverse_projection;
        p_uniform->inverse_view = inverse_view;
        p_uniform->width = width;
        p_uniform->height = height;
        p_uniform->blas_instance_count = blas_instance_count;
        p_uniform->samples = samples;
        p_uniform->p_blas_instances = p_blas_instances;
        p_uniform->p_materials = p_materials;
        p_uniform->p_bvh = p_bvh;
    }

    void render(gfx::handle_commandbuffer_t commandbuffer) {
        assert(target_width != 0 && target_height != 0 && target_image != gfx::null_handle && target_image_view != gfx::null_handle && target_sampler != gfx::null_handle);

        base_renderer.context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        base_renderer.context.cmd_begin_timer(commandbuffer, t);
        base_renderer.context.cmd_bind_pipeline(commandbuffer, p);
        base_renderer.context.cmd_bind_descriptor_sets(commandbuffer, p, 0, { base_renderer.descriptor_set(ds) });
        base_renderer.context.cmd_dispatch(commandbuffer, (target_width / 8) + 1, (target_height / 8) + 1, 1);
        base_renderer.context.cmd_end_timer(commandbuffer, t);

        static int i = 0;
        i++;
        if (i == 25) {
            i = 0;
            if (auto time = base_renderer.context.timer_get_time(t)) {
                horizon_info("{}", *time);
            } 
        }

        // maybe I dont need this transition ?
        base_renderer.context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

private:
    core::window_t& window;
    renderer::base_renderer_t& base_renderer;

    gfx::handle_descriptor_set_layout_t dsl;
    gfx::handle_pipeline_layout_t pl;
    gfx::handle_shader_t s;
    gfx::handle_pipeline_t p;
    renderer::handle_descriptor_set_t ds;
    renderer::handle_buffer_t b;
    gfx::handle_timer_t t;

    uint32_t target_width = 0, target_height = 0;
    gfx::handle_image_t target_image = gfx::null_handle;
    gfx::handle_image_view_t target_image_view = gfx::null_handle;
    gfx::handle_sampler_t target_sampler = gfx::null_handle;
};

#endif