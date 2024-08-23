#include "cvr.h"
#include "vk_ctx.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>

#define NUM_POINTS 1000000

typedef unsigned int uint;
typedef struct {
    float x, y, z;
    // unsigned char r, g, b, a; // TODO: might need to change this to a uint32_t
    // unsigned int color;
    uint color;
} Point_Vert;

typedef struct {
    Point_Vert *items;
    size_t count;
    size_t capacity;
} Vertices;

typedef struct {
    Vk_Buffer buff;
    void *data;
} Point_Cloud;

typedef struct {
    Vk_Buffer buff;
    void *data;
} Frame_Buffer;

typedef struct {
    // int img_width;
    // int img_height;
    float16 mvp;
} Point_Cloud_Uniform;

/* state we must manage */
Point_Cloud_Uniform uniform = {0};
UBO ubo = {
    .buff = {.count = 1, .size = sizeof(uniform)},
    .data = &uniform,
};
SSBO_Set ssbos = {0};
VkDescriptorSetLayout render_ds_layout;
VkDescriptorSetLayout resolve_ds_layout;
VkDescriptorSetLayout sst_ds_layout; // screen space triangle
VkPipelineLayout render_pl_layout;
VkPipelineLayout resolve_pl_layout;
VkPipelineLayout sst_pl_layout;
VkPipeline render_pl; // compute render
VkPipeline resolve_pl;
VkPipeline sst_pl;
VkDescriptorPool pool;
Vk_Texture storage_tex = {0};

typedef enum {DS_RENDER = 0, DS_RESOLVE, DS_SST, DS_TEST, DS_COUNT} DS_SET;
VkDescriptorSet ds_sets[DS_COUNT] = {0};

float rand_float()
{
    return (float)rand() / RAND_MAX;
}

uint color_to_uint(Color color)
{
    // return (((uint)color.r << 24) | ((uint)color.g << 16) | ((uint)color.b << 8) | (uint)color.a);
    return (((uint)color.a << 24) | ((uint)color.b << 16) | ((uint)color.g << 8) | (uint)color.r);
    // return (((uint)color.a << 24) | ((uint)color.r << 16) | ((uint)color.g << 8) | (uint)color.b);
}

Point_Cloud gen_points()
{
    Vertices verts = {0};
    for (size_t i = 0; i < NUM_POINTS; i++) {
        float theta = PI * rand_float();
        float phi   = 2 * PI * rand_float();
        float r     = 10.0f * rand_float();
        // Color color = color_from_HSV(r * 360.0f, 1.0f, 1.0f);
        // uint uint_color = color_to_uint(color);
        uint uint_color = color_to_uint(MAGENTA);
        Point_Vert vert = {
            .x = r * sin(theta) * cos(phi),
            .y = r * sin(theta) * sin(phi),
            .z = r * cos(theta),
            .color = uint_color,
            // .r = color.r,
            // .g = color.g,
            // .b = color.b,
            // .a = 255,
        };

        nob_da_append(&verts, vert);
    }

    Point_Cloud point_cloud = {
        .buff = {
            .count = verts.count,
            .size  = verts.count * sizeof(*verts.items),
        },
        .data = verts.items,
    };

    return point_cloud;
}

Frame_Buffer alloc_frame_buff()
{
    size_t buff_count = 2048 * 2048;
    size_t buff_size  = sizeof(uint64_t) * buff_count;
    uint64_t *data = malloc(buff_size);

    for (size_t i = 0; i < buff_count; i++) {
        data[i] = 0xffffffffff003030;
        // data[i] = 0;
    }

    Frame_Buffer frame_buff = {
        .buff = {
            .size = buff_size,
            .count = buff_count,
        },
        .data = data,
    };

    return frame_buff;
}

bool update_ubo(Camera camera)
{
    // Matrix model = {0};
    // if (!get_matrix_tos(&model)) return false;
    // Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    // Matrix proj = get_proj(camera);
    // Matrix view_proj = MatrixMultiply(view, proj);
    // Matrix mvp = MatrixMultiply(model, view_proj);
    // uniform.mvp = MatrixToFloatV(mvp);

    (void) camera;
    if (!get_mvp_float16(&uniform.mvp)) return false;
    // Window_Size win_size = get_window_size();
    // uniform.img_width = win_size.width;
    // uniform.img_height = win_size.height;

    memcpy(ubo.buff.mapped, ubo.data, ubo.buff.size);
    return true;
}

bool init_compute_buffs(Vk_Buffer buff, void *data)
{
    if (!vk_comp_buff_staged_upload(&buff, data)) return false;
    nob_da_append(&ssbos, buff);
    return true;
}

bool setup_ds_layouts()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(3,  STORAGE_IMAGE, COMPUTE_BIT)},
    };

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        /* -1 because render.comp doesn't use storage image */
        .bindingCount = NOB_ARRAY_LEN(bindings) - 1,
        .pBindings = bindings,
    };

    /* render.comp shader layout */
    if (!vk_create_ds_layout(layout_ci, &render_ds_layout)) return false;

    /* resolve.comp shader layout */
    layout_ci.bindingCount = NOB_ARRAY_LEN(bindings);
    if (!vk_create_ds_layout(layout_ci, &resolve_ds_layout)) return false;

    /* texture sampler in fragment shader */
    VkDescriptorSetLayoutBinding binding = {
        DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)
    };
    layout_ci.bindingCount = 1;
    layout_ci.pBindings = &binding;
    if (!vk_create_ds_layout(layout_ci, &sst_ds_layout)) return false;

    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 2)},
        {DS_POOL(STORAGE_BUFFER, 4)},
        {DS_POOL(COMBINED_IMAGE_SAMPLER, 1)},
        {DS_POOL(STORAGE_IMAGE, 1)},
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = NOB_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 3, // render, resolve, and screen space triangle (sst)
    };

    if (!vk_create_ds_pool(pool_ci, &pool)) return false;

    return true;
}

bool setup_ds_sets()
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetLayout layouts[] = {render_ds_layout, resolve_ds_layout, sst_ds_layout};
    size_t count = NOB_ARRAY_LEN(layouts);
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(layouts, count, pool)};
    if (!vk_alloc_ds(alloc, ds_sets)) return false;

    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = ubo.buff.handle,
        .range = ubo.buff.size,
    };
    VkDescriptorBufferInfo ssbo0_info = {
        .buffer = ssbos.items[0].handle,
        .range = ssbos.items[0].size,
    };
    VkDescriptorBufferInfo ssbo1_info = {
        .buffer = ssbos.items[1].handle,
        .range = ssbos.items[1].size,
    };
    VkDescriptorImageInfo img_info = {
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .imageView   = storage_tex.view,
        .sampler     = storage_tex.sampler,
    };
    VkWriteDescriptorSet writes[] = {
        /* render.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, ds_sets[DS_RENDER],  &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, ds_sets[DS_RENDER],  &ssbo0_info)},
        {DS_WRITE_BUFF(2, STORAGE_BUFFER, ds_sets[DS_RENDER],  &ssbo1_info)},
        /* resolve.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, ds_sets[DS_RESOLVE], &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, ds_sets[DS_RESOLVE], &ssbo0_info)},
        {DS_WRITE_BUFF(2, STORAGE_BUFFER, ds_sets[DS_RESOLVE], &ssbo1_info)},
        {DS_WRITE_IMG (3, STORAGE_IMAGE,  ds_sets[DS_RESOLVE], &img_info)},
        /* default.frag */
        {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, ds_sets[DS_SST], &img_info)},
    };
    vk_update_ds(NOB_ARRAY_LEN(writes), writes);

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
        // .position   = {0.0f, 2.0f, 5.0f},
        .position   = {20.0f, 20.0f, 20.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    if (!init_compute_buffs(point_cloud.buff, point_cloud.data)) return 1;
    if (!init_compute_buffs(frame_buff.buff, frame_buff.data)) return 1;
    free(point_cloud.data);
    free(frame_buff.data);
    if (!vk_ubo_init2(&ubo)) return 1;
    storage_tex.img.extent.width  = 2048;
    storage_tex.img.extent.height = 2048;
    if (!vk_create_storage_img(&storage_tex)) return 1;

    // /* setup descriptors */
    if (!setup_ds_layouts()) return 1;
    if (!setup_ds_pool())    return 1;
    if (!setup_ds_sets())    return 1;

    /* create pipelines */
    if (!vk_pl_layout_init(render_ds_layout, &render_pl_layout))                        return 1;
    if (!vk_compute_pl_init2("./res/render.comp.spv", render_pl_layout, &render_pl))    return 1;
    if (!vk_pl_layout_init(resolve_ds_layout, &resolve_pl_layout))                      return 1;
    if (!vk_compute_pl_init2("./res/resolve.comp.spv", resolve_pl_layout, &resolve_pl)) return 1;
    if (!vk_pl_layout_init(sst_ds_layout, &sst_pl_layout))                              return 1;
    if (!vk_basic_pl_init2(sst_pl_layout, &sst_pl))                                     return 1;

    /* record one time commands for compute buffer */
    if (!vk_rec_compute()) return 1;
        size_t group_x = ceil(ssbos.items[0].count / 128);
        vk_compute2(render_pl, render_pl_layout, ds_sets[DS_RENDER], group_x, 1, 1);
        vk_compute_pl_barrier();
        vk_compute2(resolve_pl, resolve_pl_layout, ds_sets[DS_RESOLVE], 2048 / 16, 2048 / 16, 1);
    if (!vk_end_rec_compute()) return 1;

    /* game loop */
    while (!window_should_close()) {
        update_camera_free(&camera);

        vk_submit_compute();

        begin_drawing2(); // used to update time
        vk_begin_drawing();
        /* create a barrier to ensure compute shaders are done before sampling */
        VkImageMemoryBarrier barrier = {
           .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
           .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
           .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
           .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
           .newLayout = VK_IMAGE_LAYOUT_GENERAL,
           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
           .image = storage_tex.img.handle,
           .subresourceRange = {
               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel = 0,
               .levelCount = 1,
               .baseArrayLayer = 0,
               .layerCount = 1,
           },
        };
        vk_pl_barrier(barrier);
        vk_begin_render_pass(BLACK);
        begin_mode_3d(camera);
            vk_gfx(sst_pl, sst_pl_layout, ds_sets[DS_SST]);
            rotate_y(get_time() * 0.5);
            if (!update_ubo(camera)) return 1;
        end_mode_3d();
        end_drawing();

        // begin_drawing(BLACK);
        // begin_mode_3d(camera);
        //     vk_gfx(sst_pl, sst_pl_layout, ds_sets[DS_SST]);
        //     rotate_y(get_time() * 0.5);
        //     // if (!update_ubo()) return 1;;
        // end_mode_3d();
        // end_drawing();
    }

    wait_idle();
    vk_buff_destroy(ssbos.items[0]);
    vk_buff_destroy(ssbos.items[1]);
    vk_buff_destroy(ubo.buff);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(render_ds_layout);
    vk_destroy_ds_layout(resolve_ds_layout);
    vk_destroy_ds_layout(sst_ds_layout);
    cleanup_pipeline(render_pl, render_pl_layout); // TODO: maybe vk_destroy_pl_res()?
    cleanup_pipeline(resolve_pl, resolve_pl_layout);
    cleanup_pipeline(sst_pl, sst_pl_layout);
    vk_unload_texture2(storage_tex);
    close_window();
    return 0;
}
