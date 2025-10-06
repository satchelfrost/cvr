#include "cvr.h"

typedef struct {
    Vector2 pos;
    Vector3 color;
    Vector2 uv;
} Quad_Vertex;

#define POS_UPPER_LEFT  {-0.5, -0.5}
#define POS_UPPER_RIGHT { 0.5, -0.5}
#define POS_LOWER_LEFT  {-0.5,  0.5}
#define POS_LOWER_RIGHT { 0.5,  0.5}
#define VERT_COLOR_RED   {1.0, 0.0, 0.0}
#define VERT_COLOR_GREEN {0.0, 1.0, 0.0}
#define VERT_COLOR_BLUE  {0.0, 0.0, 1.0}
#define VERT_COLOR_GRAY  {0.1, 0.1, 0.1}
#define UV_UPPER_LEFT  { 0.0,  0.0}
#define UV_UPPER_RIGHT { 1.0,  0.0}
#define UV_LOWER_LEFT  { 0.0,  1.0}
#define UV_LOWER_RIGHT { 1.0,  1.0}

#define QUAD_VTX_COUNT 4
Quad_Vertex quad_verts[QUAD_VTX_COUNT] = {
    {.pos = POS_UPPER_LEFT,  .color = VERT_COLOR_RED,  .uv = UV_UPPER_LEFT},
    {.pos = POS_UPPER_RIGHT, .color = VERT_COLOR_GREEN,.uv = UV_UPPER_RIGHT},
    {.pos = POS_LOWER_LEFT,  .color = VERT_COLOR_BLUE, .uv = UV_LOWER_LEFT},
    {.pos = POS_LOWER_RIGHT, .color = VERT_COLOR_GRAY, .uv = UV_LOWER_RIGHT},
};

/* clockwise winding order */
#define QUAD_IDX_COUNT 6
uint16_t quad_indices[QUAD_IDX_COUNT] = {
    0, 1, 2, 2, 1, 3
};

typedef struct {
    VkPipeline handle;
    VkPipelineLayout layout;
} Pipeline;

typedef enum {
    KEYPRESS_LEFT  = 1 << 0,
    KEYPRESS_RIGHT = 1 << 1,
    KEYPRESS_DOWN  = 1 << 2,
    KEYPRESS_UP    = 1 << 3,
} Keypress;

typedef struct {
    int keypress;
    int index;
    float speed;
} UBO_Data;

typedef struct {
    UBO_Data data;
    Rvk_Buffer buff;
} UBO;

#define OFFSET_COUNT 25
typedef struct {
    Vector2 xy_offsets[OFFSET_COUNT];
    Rvk_Buffer buff;
} SSBO;

void create_pipeline(Pipeline *pl, VkDescriptorSetLayout *ds_layout)
{
    rvk_create_pipeline_layout(&pl->layout, .p_set_layouts=ds_layout);
    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Quad_Vertex, pos),   },
        { .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Quad_Vertex, color), },
        { .location = 2, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Quad_Vertex, uv),    },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Quad_Vertex),
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
        .vertex_shader_name = "res/texture_distort.vert.glsl.spv",
        .fragment_shader_name = "res/texture_distort.frag.glsl.spv",
        .p_vertex_input_state = &vertex_input_ci,
        .layout = pl->layout,
    );
}

int main()
{
    init_window(600, 600, "distortion quad");

    Rvk_Buffer vtx_buff = {0};
    Rvk_Buffer idx_buff = {0};
    rvk_upload_vtx_buff(QUAD_VTX_COUNT*sizeof(Quad_Vertex), QUAD_VTX_COUNT, quad_verts, &vtx_buff);
    rvk_upload_idx_buff(QUAD_IDX_COUNT*sizeof(uint16_t), QUAD_IDX_COUNT, quad_indices, &idx_buff);

    UBO ubo = {0};
    printf("sizeof UBO_Data %zu\n", sizeof(UBO_Data));
    rvk_uniform_buff_init(sizeof(UBO_Data), &ubo.data, &ubo.buff);
    rvk_buff_map(&ubo.buff);
    memcpy(ubo.buff.mapped, &ubo.data, sizeof(UBO_Data));

    SSBO ssbo = {0};
    rvk_comp_buff_init(OFFSET_COUNT*sizeof(Vector2), OFFSET_COUNT, ssbo.xy_offsets, &ssbo.buff);
    rvk_buff_staged_upload(ssbo.buff);

    VkDescriptorSet ds;
    Rvk_Descriptor_Set_Layout ds_layout = {0};
    Rvk_Descriptor_Pool_Arena arena = {0};
    rvk_descriptor_pool_arena_init(&arena);
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    rvk_ds_layout_init(bindings, RVK_ARRAY_LEN(bindings), &ds_layout);
    rvk_descriptor_pool_arena_alloc_set(&arena, &ds_layout, &ds);
    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &ubo.buff.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &ssbo.buff.info,
        },
    };
    rvk_update_ds(RVK_ARRAY_LEN(writes), writes);

    Pipeline pl = {0};
    create_pipeline(&pl, &ds_layout.handle);

    while (!window_should_close()) {
        float dt = get_frame_time();

        if (is_key_down(KEY_LEFT_SHIFT)) ubo.data.speed = 0.1f*dt;
        else                             ubo.data.speed = 0.5f*dt;

        if (is_key_down(KEY_LEFT))  ubo.data.keypress |=  (1<<0);
        else                        ubo.data.keypress &= ~(1<<0);
        if (is_key_down(KEY_RIGHT)) ubo.data.keypress |=  (1<<1);
        else                        ubo.data.keypress &= ~(1<<1);
        if (is_key_down(KEY_DOWN))  ubo.data.keypress |=  (1<<2);
        else                        ubo.data.keypress &= ~(1<<2);
        if (is_key_down(KEY_UP))    ubo.data.keypress |=  (1<<3);
        else                        ubo.data.keypress &= ~(1<<3);

        if (is_key_pressed(KEY_SPACE) && !is_key_down(KEY_LEFT_SHIFT)) ubo.data.index = (ubo.data.index + 1) % 4;
        if (is_key_pressed(KEY_SPACE) && is_key_down(KEY_LEFT_SHIFT)) ubo.data.index = (ubo.data.index - 1 + 4) % 4;

        begin_drawing(BLACK);
            rvk_bind_gfx(pl.handle, pl.layout, &ds, 1);
            rvk_draw_buffers(vtx_buff, idx_buff);
            memcpy(ubo.buff.mapped, &ubo.data, sizeof(UBO_Data));
        end_drawing();
    }

    rvk_wait_idle();
    rvk_buff_destroy(vtx_buff);
    rvk_buff_destroy(idx_buff);
    rvk_buff_destroy(ubo.buff);
    rvk_buff_destroy(ssbo.buff);
    rvk_destroy_descriptor_set_layout(ds_layout.handle);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_destroy_pl_res(pl.handle, pl.layout);
    close_window();
    return 0;
}
