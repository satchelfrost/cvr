#include "cvr.h"

typedef struct {
    VkPipeline handle;
    VkPipelineLayout layout;
} Pipeline;

typedef struct {
    Vector3 position;
    Vector3 color;
    Vector2 uv;
} Texture_Vertex; 

void update_texture(Rvk_Texture tex, VkDescriptorSet ds)
{
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &tex.info,
    };
    rvk_update_ds(1, &write);
}

void create_sample_tex_pipeline(Pipeline *pl, VkDescriptorSetLayout *ds_layout)
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float)*16};
    rvk_create_pipeline_layout(
        &pl->layout,
        .p_push_constant_ranges = &pk_range,
        .push_constant_range_count = 1,
        .set_layout_count = 1,
        .p_set_layouts = ds_layout,
    );

    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Texture_Vertex, position), },
        { .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Texture_Vertex, color),    },
        { .location = 2, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Texture_Vertex, uv),       },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Texture_Vertex),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = RVK_ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };
    rvk_create_graphics_pipelines(
        &pl->handle,
        .vertex_shader_name = "res/texture.vert.glsl.spv",
        .fragment_shader_name = "res/texture.frag.glsl.spv",
        .p_vertex_input_state = &vertex_input_ci,
        .layout = pl->layout,
    );
}

void create_render_tex_pipeline(Pipeline *pl, VkRenderPass rp)
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float)*16};
    rvk_create_pipeline_layout(
        &pl->layout,
        .p_push_constant_ranges = &pk_range,
        .push_constant_range_count = 1,
    );

    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Texture_Vertex, position), },
        { .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Texture_Vertex, color),    },
        { .location = 2, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Texture_Vertex, uv),       },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Texture_Vertex),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = RVK_ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };
    rvk_create_graphics_pipelines(
        &pl->handle,
        .vertex_shader_name = "res/render_texture.vert.glsl.spv",
        .fragment_shader_name = "res/render_texture.frag.glsl.spv",
        .p_vertex_input_state = &vertex_input_ci,
        .layout = pl->layout,
        .render_pass = rp, 
    );
}

typedef enum {
    CAMERA_TYPE_MAIN,
    CAMERA_TYPE_VIRTUAL,
    CAMERA_TYPE_COUNT,
} Camera_Type;

int main()
{
    Camera camera = {
        .position = {1.0f, 1.0f, 1.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 90.0f,
        .projection = PERSPECTIVE,
    };
    Camera cams[CAMERA_TYPE_COUNT] = {camera, camera};
    int cam_idx = 0;

    init_window(500, 500, "render texture");
    Rvk_Descriptor_Pool_Arena arena = {0};
    rvk_descriptor_pool_arena_init(&arena);

    // pipeline for the offscreen render pass (i.e. render texture)
    VkExtent2D extent = {.width = 500, .height = 500};
    Rvk_Render_Texture render_tex = rvk_create_render_tex(extent);
    Pipeline render_tex_pl = {0};
    create_render_tex_pipeline(&render_tex_pl, render_tex.rp);
    VkDescriptorSet tex_sample_ds;
    Rvk_Descriptor_Set_Layout tex_sample_layout = {0};
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    rvk_ds_layout_init(&binding, 1, &tex_sample_layout);
    rvk_descriptor_pool_arena_alloc_set(&arena, &tex_sample_layout, &tex_sample_ds);
    update_texture(render_tex.color, tex_sample_ds);

    // pipeline that will sample the offscreen render pass
    Pipeline sample_tex_pl = {0};
    create_sample_tex_pipeline(&sample_tex_pl, &tex_sample_layout.handle);

    set_target_fps(60);
    while (!window_should_close()) {
        double t = get_time();
        if (is_key_pressed(KEY_SPACE)) cam_idx = (cam_idx + 1) % 2;
        update_camera_free(&cams[cam_idx]);

        begin_timer();
        rvk_wait_to_begin_gfx();
        rvk_begin_rec_gfx();

        begin_mode_3d(cams[CAMERA_TYPE_VIRTUAL]);
        rvk_begin_offscreen_render_pass(0.0, 0.0, 0.0, 1.0, render_tex.rp, render_tex.fb, render_tex.extent);
            rotate_y(t);
            draw_shape_ex(render_tex_pl.handle, render_tex_pl.layout, NULL, SHAPE_CUBE);
        rvk_end_render_pass();
        end_mode_3d();

        rvk_begin_render_pass(0.2, 0.3, 0.4, 1.0);
        begin_mode_3d(cams[CAMERA_TYPE_MAIN]);
            draw_shape_ex(sample_tex_pl.handle, sample_tex_pl.layout, tex_sample_ds, SHAPE_QUAD);
        end_mode_3d();
        end_drawing();
    }

    rvk_wait_idle();
    rvk_destroy_descriptor_set_layout(tex_sample_layout.handle);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_unload_texture(render_tex.depth);
    rvk_unload_texture(render_tex.color);
    rvk_destroy_render_pass(render_tex.rp);
    rvk_destroy_frame_buff(render_tex.fb);
    rvk_destroy_pl_res(sample_tex_pl.handle, sample_tex_pl.layout);
    rvk_destroy_pl_res(render_tex_pl.handle, render_tex_pl.layout);
    close_window();
    return 0;
}
