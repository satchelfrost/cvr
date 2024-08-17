#include "cvr.h"
#include "vk_ctx.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>

#define NUM_POINTS 1000000

typedef struct {
    float x, y, z;
    unsigned char r, g, b, a; // TODO: might need to change this to a uint32_t
} Point_Vert;

typedef struct {
    Point_Vert *items;
    size_t count;
    size_t capacity;
} Vertices;

typedef struct {
    Vertices verts;
    Buffer buff;
    size_t id;
} Point_Cloud;

typedef struct {
    Buffer buff;
    size_t id;
} Frame_Buffer;

typedef struct {
    float16 mvp;
    int img_width;
    int img_height;
} Point_Cloud_Uniform;

/* state we must manage */
Point_Cloud_Uniform uniform = {0};
SSBO_Set ssbos = {0};
VkDescriptorSetLayout render_layout;
VkDescriptorSetLayout resolve_layout;
VkDescriptorPool pool;

void log_fps()
{
    static int fps = -1;
    int curr_fps = get_fps();
    if (curr_fps != fps) {
        nob_log(NOB_INFO, "FPS %d", curr_fps);
        fps = curr_fps;
    }
}

float rand_float()
{
    return (float)rand() / RAND_MAX;
}

Point_Cloud gen_points()
{
    Vertices verts = {0};
    for (size_t i = 0; i < NUM_POINTS; i++) {
        float theta = PI * rand_float();
        float phi   = 2 * PI * rand_float();
        float r     = 10.0f * rand_float();
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

        nob_da_append(&verts, vert);
    }

    Point_Cloud point_cloud = {
        .buff.items = verts.items,
        .buff.count = verts.count,
        .buff.size  = verts.count * sizeof(*verts.items),
        .verts = verts,
    };

    return point_cloud;
}

Frame_Buffer alloc_frame_buff()
{
    size_t buff_count = 1024 * 1024 * 4;
    size_t buff_size  = sizeof(uint64_t) * buff_count;
    uint64_t *data = malloc(buff_size);

    Frame_Buffer frame_buff = {
        .buff.size = buff_size,
        .buff.items = data,
        .buff.count = buff_count,
    };

    return frame_buff;
}

bool update_ubo()
{
    if (!get_mvp_float16(&uniform.mvp)) return false;
    Window_Size win_size = get_window_size();
    uniform.img_width = win_size.width;
    uniform.img_height = win_size.height;

    return true;
}

bool init_compute_buffs(Buffer buff)
{
    Vk_Buffer vk_buff = { .count = buff.count, .size  = buff.size, };
    if (!vk_comp_buff_staged_upload(&vk_buff, buff.items)) return false;
    nob_da_append(&ssbos, vk_buff);
    return true;
}

bool setup_ds_layout()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        { DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT) },
        { DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT) },
        { DS_BINDING(2, STORAGE_BUFFER, COMPUTE_BIT) },
        { DS_BINDING(3,  STORAGE_IMAGE, COMPUTE_BIT) },
    };

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        /* -1 because render.comp doesn't use storage image */
        .bindingCount = NOB_ARRAY_LEN(bindings) - 1,
        .pBindings = bindings,
    };

    /* render.comp shader layout */
    if (!vk_create_ds_layout(layout_ci, &render_layout)) return false;

    /* resolve.comp shader layout */
    layout_ci.bindingCount = NOB_ARRAY_LEN(bindings);
    if (!vk_create_ds_layout(layout_ci, &resolve_layout)) return false;

    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        { DS_POOL(UNIFORM_BUFFER, 1) },
        { DS_POOL(STORAGE_BUFFER, 2) },
        { DS_POOL(COMBINED_IMAGE_SAMPLER, 1) },
        { DS_POOL(STORAGE_IMAGE, 1) },
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = NOB_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 3,
    };

    if (!vk_create_ds_pool(pool_ci, &pool)) return false;

    return true;
}

int main()
{
    Point_Cloud point_cloud = gen_points();
    Frame_Buffer frame_buff = alloc_frame_buff();

    /* initialize window and Vulkan */
    init_window(1600, 900, "compute based rasterization for a point cloud");
    set_target_fps(60);
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    // if (!ssbo_init(EXAMPLE_COMPUTE_RASTERIZER)) return 1;
    // if (!vk_ssbo_descriptor_set_layout_init(DS_TYPE_COMPUTE_RASTERIZER)) return false;
    // if (!vk_ssbo_descriptor_pool_init(DS_TYPE_COMPUTE_RASTERIZER))       return false;
    // if (!vk_ssbo_descriptor_set_init(DS_TYPE_COMPUTE_RASTERIZER))        return false;

    if (!init_compute_buffs(point_cloud.buff)) return 1;
    if (!init_compute_buffs(frame_buff.buff)) return 1;
    nob_da_free(point_cloud.verts);
    free(frame_buff.buff.items);

    if (!setup_ds_layout()) return 1;
    if (!setup_ds_pool()) return 1;

    nob_log(NOB_INFO, "before end break");
    goto end;

    /* initialize and map the uniform data */
    Buffer buff = {
        .size  = sizeof(uniform),
        .count = 1,
        .items = &uniform,
    };
    if (!ubo_init(buff, EXAMPLE_COMPUTE_RASTERIZER)) return 1;

    /* initialize the storage image */
    Window_Size win_size = get_window_size();
    int width = win_size.width;
    int height = win_size.height;
    if (!storage_img_init(width, height, EXAMPLE_COMPUTE_RASTERIZER)) return 1;

    while (!window_should_close()) {
        update_camera_free(&camera);

        begin_compute();
            if (!compute(EXAMPLE_COMPUTE_RASTERIZER)) return 1;
        end_compute();

        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(get_time() * 0.5);
            if (!draw_sst()) return 1;
            if (!update_ubo()) return 1;;
        end_mode_3d();
        end_drawing();
    }

end:
    nob_log(NOB_INFO, "early debug end");
    wait_idle();
    vk_buff_destroy(ssbos.items[0]);
    vk_buff_destroy(ssbos.items[1]);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(render_layout);
    vk_destroy_ds_layout(resolve_layout);
    close_window();
    return 0;
}
