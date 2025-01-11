#include "cvr.h"
#include "ext/nob.h"

#define MAX_POINTS  100000000 // 100 million
#define MIN_POINTS  100000    // 100 thousand
#define MAX_FPS_REC 1000

typedef struct {
    float x, y, z;
    unsigned char r, g, b, a;
} Point_Vert;

typedef struct {
    Point_Vert *items;
    size_t count;
    size_t capacity;
    Vk_Buffer buff;
    size_t id;
    bool pending_change;
    const size_t max;
    const size_t min;
} Point_Cloud;

typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
    const size_t max;
    bool collecting;
} FPS_Record;

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

void gen_points(size_t num_points, Point_Cloud *pc)
{
    /* reset the point count to zero, but leave capacity allocated */
    pc->count = 0;

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

        nob_da_append(pc, vert);
    }

    pc->buff.count = pc->count;
    pc->buff.size  = pc->count * sizeof(*pc->items);
}

bool create_pipeline()
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
    if (!vk_pl_layout_init(layout_ci, &gfx_pl_layout))
        return false;

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
        .vert = "./res/point-cloud.vert.spv",
        .frag = "./res/point-cloud.frag.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        .polygon_mode = VK_POLYGON_MODE_POINT,
        .vert_attrs = vert_attrs,
        .vert_attr_count = NOB_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!vk_basic_pl_init(config, &gfx_pl)) return false;

    return true;
}

int main()
{
    /* generate a point cloud */
    size_t num_points = MIN_POINTS;
    Point_Cloud pc = {.min = MIN_POINTS, .max = MAX_POINTS};
    gen_points(num_points, &pc);
    FPS_Record record = {.max = MAX_FPS_REC};

    /* initialize window and Vulkan */
    init_window(1600, 900, "point cloud");
    Camera camera = {
        .position   = {10.0f, 10.0f, 10.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    if (!vk_vtx_buff_staged_upload(&pc.buff, pc.items)) return 1;

    if (!create_pipeline()) return 1;

    while (!window_should_close()) {
        /* input */
        update_camera_free(&camera);
        if (is_key_pressed(KEY_UP)) {
            if (num_points * 10 <= pc.max) {
                pc.pending_change = true;
                num_points = num_points * 10;
            } else {
                nob_log(NOB_INFO, "max point count reached");
            }
        }
        if (is_key_pressed(KEY_DOWN)) {
            if (num_points / 10 >= pc.min) {
                pc.pending_change = true;
                num_points = num_points / 10;
            } else {
                nob_log(NOB_INFO, "min point count reached");
            }
        }
        if (is_key_pressed(KEY_R)) record.collecting = true;

        /* upload point cloud if we've changed points */
        if (pc.pending_change) {
            vk_wait_idle();
            vk_buff_destroy(&pc.buff);
            gen_points(num_points, &pc);
            if (!vk_vtx_buff_staged_upload(&pc.buff, pc.items)) return 1;
            pc.pending_change = false;
        }

        /* collect the frame rate */
        if (record.collecting) {
            nob_da_append(&record, get_fps());
            if (record.count >= record.max) {
                /* print results and reset */
                size_t sum = 0;
                for (size_t i = 0; i < record.count; i++) sum += record.items[i];
                float ave = (float) sum / record.count;
                nob_log(NOB_INFO, "Average (N=%zu) FPS %.2f, %zu points", record.count, ave, pc.count);
                record.count = 0;
                record.collecting = false;
            }
        }

        /* drawing */
        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(get_time() * 0.5);
            if (!draw_points(pc.buff, gfx_pl, gfx_pl_layout, NULL, 0)) return 1;
        end_mode_3d();
        end_drawing();
    }

    vk_wait_idle();
    vk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    vk_buff_destroy(&pc.buff);
    nob_da_free(pc);
    close_window();
    return 0;
}
