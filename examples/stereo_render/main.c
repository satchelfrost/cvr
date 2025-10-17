#include "cvr.h"

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 500

typedef struct {
    VkPipeline pl;
    VkPipelineLayout pl_layout;
    Rvk_Descriptor_Set_Layout ds_layout;
    VkDescriptorSet ds;
} Pipeline;

typedef struct {
    Vector3 position;
    Vector3 color;
    Vector2 uv;
} Basic_Vertex;

Pipeline multiview = {0};
Pipeline viewdisplay = {0};
Rvk_Descriptor_Pool_Arena arena = {0};

/* two for stereo*/
#define VIEW_COUNT 2

typedef struct {
    float16 projection[VIEW_COUNT];
    float16 model_view[VIEW_COUNT];
} Uniform_Data;

typedef struct {
    Rvk_Buffer buffer;
    Uniform_Data data;
} Uniform;

void setup_ds_layouts()
{
    VkDescriptorSetLayoutBinding multiview_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
    };
    rvk_ds_layout_init(multiview_bindings, RVK_ARRAY_LEN(multiview_bindings), &multiview.ds_layout);

    VkDescriptorSetLayoutBinding viewdisplay_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    rvk_ds_layout_init(viewdisplay_bindings, RVK_ARRAY_LEN(viewdisplay_bindings), &viewdisplay.ds_layout);
}

void update_ds(Rvk_Buffer uniform_buffer, Rvk_Texture render_texture)
{
    rvk_descriptor_pool_arena_alloc_set(&arena, &multiview.ds_layout,  &multiview.ds);
    rvk_descriptor_pool_arena_alloc_set(&arena, &viewdisplay.ds_layout,  &viewdisplay.ds);

    VkWriteDescriptorSet writes[] = {
        { /* multiview.vert.glsl */
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet = multiview.ds,
            .pBufferInfo = &uniform_buffer.info,
        },
        { /* viewdisplay.vert.glsl */
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .dstSet = viewdisplay.ds,
            .pImageInfo = &render_texture.info,
        },
    };
    rvk_update_ds(RVK_ARRAY_LEN(writes), writes);
}

void create_multiview_pl(VkRenderPass rp)
{
    rvk_create_pipeline_layout(&multiview.pl_layout, .p_set_layouts = &multiview.ds_layout.handle);
    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Basic_Vertex, position), },
        { .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Basic_Vertex, color),    },
        { .location = 2, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Basic_Vertex, uv),       },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Basic_Vertex),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = RVK_ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };
    rvk_create_graphics_pipelines(
        &multiview.pl,
        .layout = multiview.pl_layout,
        .render_pass = rp,
        .p_vertex_input_state = &vertex_input_ci,
        .vertex_shader_name = "res/multiview.vert.glsl.spv",
        .fragment_shader_name = "res/multiview.frag.glsl.spv",
    );
}

void create_viewdisplay_pl()
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .size = sizeof(uint32_t)};
    rvk_create_pipeline_layout(
        &viewdisplay.pl_layout,
        .p_set_layouts = &viewdisplay.ds_layout.handle,
        .p_push_constant_ranges = &pk_range,
    );
    VkPipelineVertexInputStateCreateInfo empty_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    rvk_create_graphics_pipelines(
        &viewdisplay.pl,
        .layout = viewdisplay.pl_layout,
        .p_vertex_input_state = &empty_input_state,
        .vertex_shader_name = "res/viewdisplay.vert.glsl.spv",
        .fragment_shader_name = "res/viewdisplay.frag.glsl.spv",
    );
}

void create_pipelines(VkRenderPass multiview_rp)
{
    create_multiview_pl(multiview_rp);
    create_viewdisplay_pl();
}

void update_uniform(Uniform *uniform, Camera camera)
{
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    Matrix projection = get_proj(camera);
    // for now just use two identical views
    uniform->data.projection[0] = MatrixToFloatV(projection);
    uniform->data.model_view[0] = MatrixToFloatV(view);
    uniform->data.projection[1] = MatrixToFloatV(projection);
    uniform->data.model_view[1] = MatrixToFloatV(view);
    memcpy(uniform->buffer.mapped, &uniform->data, sizeof(Uniform_Data));
}

void draw_shape_multiview(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds, Shape_Type shape)
{
    Rvk_Buffer vtx_buff = get_shape_vertex_buffer(shape);
    Rvk_Buffer idx_buff = get_shape_index_buffer(shape);
    rvk_cmd_bind_pipeline(pl, VK_PIPELINE_BIND_POINT_GRAPHICS);
    rvk_cmd_bind_descriptor_sets(pl_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, &ds);
    VkViewport viewport = { .width  = WINDOW_HEIGHT, .height = WINDOW_HEIGHT, .maxDepth = 1.0f };
    VkRect2D scissor = { .extent = {.width = WINDOW_HEIGHT, .height = WINDOW_HEIGHT}};
    rvk_cmd_set_viewport(viewport);
    rvk_cmd_set_scissor(scissor);
    rvk_draw_buffers(vtx_buff, idx_buff);
}

int main()
{
    Camera camera = {
        .position = {2.0f, 2.0f, 2.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = PERSPECTIVE,
    };

    rvk_enable_multiview_feature();
    init_window(WINDOW_WIDTH, WINDOW_HEIGHT, "stereo render");

    VkExtent2D extent = {WINDOW_HEIGHT, WINDOW_HEIGHT};
    Rvk_Render_Texture render_texture = rvk_create_multiview_render_texture(extent, 2);
    Uniform uniform = {0};
    uniform.buffer = rvk_create_mapped_uniform_buff(sizeof(Uniform_Data), &uniform.data);

    rvk_descriptor_pool_arena_init(&arena);
    setup_ds_layouts();
    update_ds(uniform.buffer, render_texture.color);
    create_pipelines(render_texture.rp);

    set_target_fps(120);
    Color color = BLUE;
    while (!window_should_close()) {
        update_camera_free(&camera);

        begin_frame();
            update_uniform(&uniform, camera);
            rvk_begin_offscreen_render_pass(
                color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f,
                render_texture.rp,
                render_texture.fb,
                render_texture.extent);
                begin_mode_3d(camera);
                    draw_shape_multiview(multiview.pl, multiview.pl_layout, multiview.ds, SHAPE_CUBE);
                end_mode_3d();
            rvk_end_render_pass();

            rvk_begin_render_pass(0.0f, 0.0f, 0.0f, 1.0f);
                rvk_cmd_bind_pipeline(viewdisplay.pl, VK_PIPELINE_BIND_POINT_GRAPHICS);
                rvk_cmd_bind_descriptor_sets(viewdisplay.pl_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, &viewdisplay.ds);

                VkViewport viewport = {
                    .width    = WINDOW_WIDTH/2.0f,
                    .height   = WINDOW_HEIGHT,
                    .maxDepth = 1.0f,
                };
                VkRect2D scissor = { .extent = {.width = WINDOW_WIDTH/2.0f, .height = WINDOW_HEIGHT}};

                /* left eye */
                rvk_cmd_set_viewport(viewport);
                rvk_cmd_set_scissor(scissor);
                uint32_t eye = 0;
                rvk_push_const(viewdisplay.pl_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t), &eye);
                rvk_cmd_draw(3);

                /* right eye */
                viewport.x = WINDOW_WIDTH/2.0f;
                scissor.offset.x = WINDOW_WIDTH/2.0f;
                rvk_cmd_set_viewport(viewport);
                rvk_cmd_set_scissor(scissor);
                eye++;
                rvk_push_const(viewdisplay.pl_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t), &eye);
                rvk_cmd_draw(3);
            rvk_end_render_pass();
        end_frame();
    }

    rvk_wait_idle();

    rvk_buff_destroy(uniform.buffer);
    rvk_destroy_descriptor_set_layout(multiview.ds_layout.handle);
    rvk_destroy_descriptor_set_layout(viewdisplay.ds_layout.handle);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_destroy_pl_res(multiview.pl, multiview.pl_layout);
    rvk_destroy_pl_res(viewdisplay.pl, viewdisplay.pl_layout);
    rvk_destroy_render_texture(render_texture);

    close_window();

    return 0;
}
