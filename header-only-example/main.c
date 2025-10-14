#define VK_VALIDATION
#define PLATFORM_DESKTOP_GLFW
#define RAG_VK_IMPLEMENTATION
#include "../src/rag_vk.h"

typedef struct {
    float x;
    float y;
    float z;
} Vector3;

typedef struct {
    float x;
    float y;
} Vector2;

typedef struct {
    Vector3 pos;
    Vector3 color;
} Vertex; 

#define QUAD_VERT_COUNT 4
Vertex quad_verts[QUAD_VERT_COUNT] = {
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}
};

#define QUAD_IDX_COUNT 6
uint16_t quad_indices[QUAD_IDX_COUNT] = {
    0, 1, 2, 2, 3, 0
};

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

void create_pipeline()
{
    /* create pipeline layout */
    VkPipelineLayoutCreateInfo layout_ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    rvk_pl_layout_init(layout_ci, &gfx_pl_layout);

    /* create pipeline */
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .location = 0,
            .offset = offsetof(Vertex, pos),
        },
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .location = 1,
            .offset = offsetof(Vertex, color),
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vertex),
    };
    Pipeline_Config config = {
        .pl_layout = gfx_pl_layout,
        .vert = "default.vert.spv",
        .frag = "default.frag.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = RVK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    rvk_basic_pl_init(config, &gfx_pl);
}

int main()
{
    rvk_init();
    create_pipeline();

    /* upload resources to GPU */
    Rvk_Buffer quad_vtx_buff = {0};
    Rvk_Buffer quad_idx_buff = {0};
    rvk_vtx_buff_init(QUAD_VERT_COUNT * sizeof(Vertex),  QUAD_VERT_COUNT, quad_verts, &quad_vtx_buff);
    rvk_idx_buff_init(QUAD_IDX_COUNT * sizeof(uint16_t), QUAD_IDX_COUNT, quad_indices, &quad_idx_buff);
    rvk_buff_staged_upload(quad_vtx_buff);
    rvk_buff_staged_upload(quad_idx_buff);

    int esc = 0;
    do {
        rvk_wait_to_begin_gfx();
            rvk_begin_rec_gfx();
                rvk_begin_render_pass(0.0, 1.0, 1.0, 1.0);
                    rvk_bind_gfx(gfx_pl, gfx_pl_layout, NULL, 0);
                    rvk_draw_buffers(quad_vtx_buff, quad_idx_buff);
                rvk_end_render_pass();
            rvk_end_rec_gfx();
        rvk_submit_gfx();

        glfwPollEvents();
        esc = glfwGetKey(rvk_glfw_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    } while (!esc && !glfwWindowShouldClose(rvk_glfw_window));

    /* cleanup */
    vkDeviceWaitIdle(rvk_ctx.device);
    rvk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    rvk_buff_destroy(quad_vtx_buff);
    rvk_buff_destroy(quad_idx_buff);
    rvk_destroy();
    rvk_glfw_destroy();

    return 0;
}
