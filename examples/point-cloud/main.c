#include "cvr.h"
#include "geometry.h"

#define POINT_COUNT (1000*1000)

typedef struct {
    float x, y, z;
    unsigned char r, g, b, a;
} Point_Vert;

typedef struct {
    Point_Vert *data;
    Rvk_Buffer buff;
} Point_Cloud;

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

Point_Cloud generate_point_cloud(size_t num_points)
{
    Point_Cloud pc = {0};
    pc.buff.count = num_points;
    pc.buff.size  = num_points * sizeof(Point_Cloud);
    pc.data       = malloc(pc.buff.size);

    for (size_t i = 0; i < num_points; i++) {
        float theta = PI * rand() / RAND_MAX;
        float phi   = 2.0f * PI * rand() / RAND_MAX;
        float r     = 10.0f * rand() / RAND_MAX;
        Color color = color_from_HSV(r * 360.0f, 1.0f, 1.0f);
        Point_Vert vert = {
            .x = r * sin(theta) * cos(phi),
            .y = r * sin(theta) * sin(phi),
            .z = r * cos(theta),
            .r = color.r,
            .g = color.g,
            .b = color.b,
            .a = 255,
        };
        pc.data[i] = vert;
    }

    return pc;
}

void create_pipeline()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(float16),
    };
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    rvk_pl_layout_init(layout_ci, &gfx_pl_layout);

    /* create pipeline */
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .location = 1,
            .format = VK_FORMAT_R8G8B8A8_UINT,
            .offset = offsetof(Small_Vertex, r),
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Small_Vertex),
    };
    Pipeline_Config config = {
        .pl_layout = gfx_pl_layout,
        .vert = "./res/point-cloud.vert.glsl.spv",
        .frag = "./res/point-cloud.frag.glsl.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        .polygon_mode = VK_POLYGON_MODE_POINT,
        .vert_attrs = vert_attrs,
        .vert_attr_count = RVK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    rvk_basic_pl_init(config, &gfx_pl);
}

int main()
{
    Camera camera = {
        .position   = {10.0f, 10.0f, 10.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    Point_Cloud pc = generate_point_cloud(POINT_COUNT);

    /* initialize window and Vulkan */
    init_window(1600, 900, "point cloud");

    /* upload resources to GPU */
    rvk_vtx_buff_init(pc.buff.size, pc.buff.count, pc.data, &pc.buff);
    rvk_buff_staged_upload(pc.buff);
    create_pipeline();

    while (!window_should_close()) {
        update_camera_free(&camera);
        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(get_time() * 0.5);
            if (!draw_points(pc.buff, gfx_pl, gfx_pl_layout, NULL, 0)) return 1;
        end_mode_3d();
        end_drawing();
    }

    rvk_wait_idle();
    rvk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    rvk_buff_destroy(pc.buff);
    free(pc.data);
    close_window();
    return 0;
}
