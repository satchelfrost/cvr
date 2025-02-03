#define ENABLE_VALIDATION
#define PLATFORM_DESKTOP_GLFW
#define VK_CTX_IMPLEMENTATION
#include "../src/vk_ctx.h"

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
const Vertex quad_verts[QUAD_VERT_COUNT] = {
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}
};

#define QUAD_IDX_COUNT 6
const uint16_t quad_indices[QUAD_IDX_COUNT] = {
    0, 1, 2, 2, 3, 0
};

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

bool create_pipeline()
{
    /* create pipeline layout */
    VkPipelineLayoutCreateInfo layout_ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (!vk_pl_layout_init(layout_ci, &gfx_pl_layout)) return false;

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
        .vert_attr_count = VK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!vk_basic_pl_init(config, &gfx_pl)) return false;

    return true;
}

int main()
{
    /* initialize glfw */
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    platform.handle = glfwCreateWindow(500, 500, "basic vulkan", NULL, NULL);
    if (!platform.handle) return 1;

    /* initialize vulkan */
    if (!vk_init()) return 1;
    if (!create_pipeline()) return 1;

    /* upload resources to GPU */
    Vk_Buffer quad_vtx_buff = {
        .size  = QUAD_VERT_COUNT * sizeof(Vertex),
        .count = QUAD_VERT_COUNT,
    };
    Vk_Buffer quad_idx_buff = {
        .size  = QUAD_IDX_COUNT * sizeof(Vertex),
        .count = QUAD_IDX_COUNT,
    };
    if (!vk_vtx_buff_upload(&quad_vtx_buff, quad_verts))   return 1;
    if (!vk_idx_buff_upload(&quad_idx_buff, quad_indices)) return 1;

    int esc = 0;
    do {
        vk_wait_to_begin_gfx();
            vk_begin_rec_gfx();
                vk_begin_render_pass(0.0, 1.0, 1.0, 1.0);
                    vk_bind_gfx(gfx_pl, gfx_pl_layout, NULL, 0);
                    vk_draw_buffers(quad_vtx_buff, quad_idx_buff);
                vk_end_render_pass();
            vk_end_rec_gfx();
        vk_submit_gfx();

        glfwPollEvents();
        esc = glfwGetKey(platform.handle, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    } while (!esc && !glfwWindowShouldClose(platform.handle));

    /* cleanup */
    vkDeviceWaitIdle(vk_ctx.device);
    vk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    vk_buff_destroy(&quad_vtx_buff);
    vk_buff_destroy(&quad_idx_buff);
    vk_destroy();
    glfwDestroyWindow(platform.handle);
    glfwTerminate();

    return 0;
}
