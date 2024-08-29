#include "cvr.h"
#include "vk_ctx.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>

#define MAX_POINTS 100000000 // 100 million
#define MIN_POINTS 100000    // 100 thousand
#define FRAME_BUFF_SZ 2048
#define MAX_FPS_REC 100
#define SUBGROUP_SZ 1024     // Subgroup size for render.comp
#define NUM_BATCHES 8        // Number of batches to dispatch

#define read_attr(attr, sv)                   \
    do {                                      \
        memcpy(&attr, sv.data, sizeof(attr)); \
        sv.data  += sizeof(attr);             \
        sv.count -= sizeof(attr);             \
    } while(0)

typedef unsigned int uint;
typedef struct {
    float x, y, z;
    uint color;
} Point_Vert;

typedef struct {
    Point_Vert *items;
    size_t count;
    size_t capacity;
    Vk_Buffer buff;
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

typedef struct {
    Vk_Buffer buff;
    void *data;
} Frame_Buffer;

typedef struct {
    float16 mvp;
} UBO_Data;

typedef struct {
    Vk_Buffer buff;
    UBO_Data data;
} Point_Cloud_UBO;

/* Vulkan state */
VkDescriptorSetLayout cs_render_ds_layout;
VkDescriptorSetLayout cs_resolve_ds_layout;
VkDescriptorSetLayout gfx_ds_layout;
VkPipelineLayout cs_render_pl_layout;
VkPipelineLayout cs_resolve_pl_layout;
VkPipelineLayout gfx_pl_layout;
VkPipeline cs_render_pl;
VkPipeline cs_resolve_pl;
VkPipeline gfx_pl;
VkDescriptorPool pool;

const char *point_cloud_files[] = {
    "res/arena_5060224_f32.vtx",
    "res/arena_50602232_f32.vtx",
    // "res/arena_101204464_f32.vtx",
    // "res/arena_506022320_f32.vtx",
};

typedef enum {DS_RENDER = 0, DS_RESOLVE, DS_SST, DS_COUNT} DS_SET;
VkDescriptorSet ds_sets[DS_COUNT] = {0};

bool load_points(const char *file, Point_Cloud *verts)
{
    bool result = true;

    /* reset count to zero in case we are loading a new point cloud */
    verts->count = 0;

    nob_log(NOB_INFO, "reading vtx file %s", file);
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(file, &sb)) nob_return_defer(false);

    Nob_String_View sv = nob_sv_from_parts(sb.items, sb.count);
    size_t vtx_count = 0;
    read_attr(vtx_count, sv);

    for (size_t i = 0; i < vtx_count; i++) {
        float x, y, z;
        uint8_t r, g, b;
        read_attr(x, sv);
        read_attr(y, sv);
        read_attr(z, sv);
        read_attr(r, sv);
        read_attr(g, sv);
        read_attr(b, sv);

        uint uint_color = (uint)255 << 24 | (uint)b << 16 | (uint)g << 8 | (uint)r;
        Point_Vert vert = {
            .x = x, .y = y, .z = z,
            .color = uint_color,
        };
        nob_da_append(verts, vert);
    }

    verts->buff.count = vtx_count;
    verts->buff.size = sizeof(Point_Vert) * vtx_count;

defer:
    nob_sb_free(sb);
    return result;
}

Frame_Buffer alloc_frame_buff()
{
    Frame_Buffer frame = {0};
    frame.buff.count = FRAME_BUFF_SZ * FRAME_BUFF_SZ;
    frame.buff.size  = sizeof(uint64_t) * frame.buff.count;
    frame.data = malloc(frame.buff.size);
    return frame;
}

bool setup_ds_layouts()
{
    /* render.comp shader layout */
    VkDescriptorSetLayoutBinding cs_render_bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2, STORAGE_BUFFER, COMPUTE_BIT)},
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = NOB_ARRAY_LEN(cs_render_bindings),
        .pBindings = cs_render_bindings,
    };
    if (!vk_create_ds_layout(layout_ci, &cs_render_ds_layout)) return false;

    /* resolve.comp shader layout */
    VkDescriptorSetLayoutBinding cs_resolve_bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2,  STORAGE_IMAGE, COMPUTE_BIT)},
    };
    layout_ci.bindingCount = NOB_ARRAY_LEN(cs_resolve_bindings);
    layout_ci.pBindings = cs_resolve_bindings;
    if (!vk_create_ds_layout(layout_ci, &cs_resolve_ds_layout)) return false;

    /* texture sampler in fragment shader */
    VkDescriptorSetLayoutBinding gfx_binding = {
        DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)
    };
    layout_ci.bindingCount = 1;
    layout_ci.pBindings = &gfx_binding;
    if (!vk_create_ds_layout(layout_ci, &gfx_ds_layout)) return false;

    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 2)},
        {DS_POOL(STORAGE_BUFFER, 3)},
        {DS_POOL(COMBINED_IMAGE_SAMPLER, 1)},
        {DS_POOL(STORAGE_IMAGE, 1)},
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = NOB_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 3, // compute shader render, compute shader resolve, and graphics
    };

    if (!vk_create_ds_pool(pool_ci, &pool)) return false;

    return true;
}

bool setup_ds_sets(Vk_Buffer ubo, Vk_Buffer point_cloud, Vk_Buffer frame_buff, Vk_Texture storage_tex)
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetLayout layouts[] = {cs_render_ds_layout, cs_resolve_ds_layout, gfx_ds_layout};
    size_t count = NOB_ARRAY_LEN(layouts);
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(layouts, count, pool)};
    if (!vk_alloc_ds(alloc, ds_sets)) return false;

    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = ubo.handle,
        .range  = ubo.size,
    };
    VkDescriptorBufferInfo pc_info = {
        .buffer = point_cloud.handle,
        .range  = point_cloud.size,
    };
    VkDescriptorBufferInfo frame_buff_info = {
        .buffer = frame_buff.handle,
        .range  = frame_buff.size,
    };
    VkDescriptorImageInfo img_info = {
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .imageView   = storage_tex.view,
        .sampler     = storage_tex.sampler,
    };
    VkWriteDescriptorSet writes[] = {
        /* render.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, ds_sets[DS_RENDER],  &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, ds_sets[DS_RENDER],  &pc_info)},
        {DS_WRITE_BUFF(2, STORAGE_BUFFER, ds_sets[DS_RENDER],  &frame_buff_info)},
        /* resolve.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, ds_sets[DS_RESOLVE], &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, ds_sets[DS_RESOLVE], &frame_buff_info)},
        {DS_WRITE_IMG (2, STORAGE_IMAGE,  ds_sets[DS_RESOLVE], &img_info)},
        /* default.frag */
        {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, ds_sets[DS_SST], &img_info)},
    };
    vk_update_ds(NOB_ARRAY_LEN(writes), writes);

    return true;
}

bool build_compute_cmds(size_t point_cloud_count)
{
    size_t group_x = 1; size_t group_y = 1; size_t group_z = 1;
    if (!vk_rec_compute()) return false;
        /* submit batches of points to render compute shader */
        group_x = point_cloud_count / SUBGROUP_SZ + 1;
        size_t batch_size = group_x / NUM_BATCHES;
        for (size_t i = 0; i < NUM_BATCHES; i++) {
            uint32_t offset = i * batch_size * SUBGROUP_SZ;
            vk_push_const(cs_render_pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &offset);
            vk_compute(cs_render_pl, cs_render_pl_layout, ds_sets[DS_RENDER], batch_size, group_y, group_z);
        }

        vk_compute_pl_barrier();

        /* resolve the frame buffer */
        group_x = group_y = FRAME_BUFF_SZ / 16;
        vk_compute(cs_resolve_pl, cs_resolve_pl_layout, ds_sets[DS_RESOLVE], group_x, group_y, group_z);
    if (!vk_end_rec_compute()) return false;
    return true;
}

int main()
{
    Point_Cloud pc = {.min = MIN_POINTS, .max = MAX_POINTS};
    Vk_Texture storage_tex = {.img.extent = {FRAME_BUFF_SZ, FRAME_BUFF_SZ}};
    Frame_Buffer frame = alloc_frame_buff();
    Point_Cloud_UBO ubo = {.buff = {.count = 1, .size = sizeof(UBO_Data)}};
    FPS_Record record = {.max = MAX_FPS_REC};
    int pc_idx = 0;
    if (!load_points(point_cloud_files[pc_idx], &pc)) return 1;
    if (!load_points(point_cloud_files[pc_idx], &pc)) return 1;

    /* initialize window and Vulkan */
    init_window(1600, 900, "arena point cloud rasterization");
    Camera camera = {
        .position   = {10.0f, 10.0f, 10.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    if (!vk_comp_buff_staged_upload(&pc.buff, pc.items))      return 1;
    if (!vk_comp_buff_staged_upload(&frame.buff, frame.data)) return 1;
    if (!vk_ubo_init2(&ubo.buff))                             return 1;
    if (!vk_create_storage_img(&storage_tex))                 return 1;

    /* setup descriptors */
    if (!setup_ds_layouts())                                        return 1;
    if (!setup_ds_pool())                                           return 1;
    if (!setup_ds_sets(ubo.buff, pc.buff, frame.buff, storage_tex)) return 1;

    /* create pipelines */
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(uint32_t)};
    if (!vk_pl_layout_init2(cs_render_ds_layout, &cs_render_pl_layout, 1, &pk_range))        return 1;
    if (!vk_compute_pl_init("./res/render.comp.spv", cs_render_pl_layout, &cs_render_pl))    return 1;
    if (!vk_pl_layout_init(cs_resolve_ds_layout, &cs_resolve_pl_layout))                     return 1;
    if (!vk_compute_pl_init("./res/resolve.comp.spv", cs_resolve_pl_layout, &cs_resolve_pl)) return 1;
    if (!vk_pl_layout_init(gfx_ds_layout, &gfx_pl_layout))                                   return 1;
    if (!vk_basic_pl_init2(gfx_pl_layout, &gfx_pl))                                          return 1;

    /* record commands for compute buffer */
    if (!build_compute_cmds(pc.count)) return 1;

    /* game loop */
    while (!window_should_close()) {
        /* input */
        update_camera_free(&camera);
        if (is_key_pressed(KEY_UP)) {
            pc_idx = (pc_idx + 1) % NOB_ARRAY_LEN(point_cloud_files);
            pc.pending_change = true;
        }
        if (is_key_pressed(KEY_DOWN)) {
            pc_idx = (pc_idx - 1 + NOB_ARRAY_LEN(point_cloud_files)) % NOB_ARRAY_LEN(point_cloud_files);
            pc.pending_change = true;
        }
        if (is_key_pressed(KEY_R)) record.collecting = true;

        /* re-upload point cloud if we've changed point cloud size */
        if (pc.pending_change) {
            /* destroy old point cloud buffer and generate new points */
            wait_idle();
            vk_buff_destroy(pc.buff);
            if (!load_points(point_cloud_files[pc_idx], &pc)) return 1;

            /* upload new buffer and update descriptor sets */
            if (!vk_comp_buff_staged_upload(&pc.buff, pc.items)) return 1;
            VkDescriptorBufferInfo pc_info = {.buffer = pc.buff.handle, .range = pc.buff.size};
            VkWriteDescriptorSet write = {DS_WRITE_BUFF(1, STORAGE_BUFFER, ds_sets[DS_RENDER],  &pc_info)};
            vk_update_ds(1, &write);

            /* rebuild compute commands */
            if (!build_compute_cmds(pc.count)) return 1;
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

        /* submit compute commands */
        if (!vk_submit_compute()) return 1;

        /* render loop */
        start_timer();
        if (!vk_begin_drawing()) return 1;
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
            vk_gfx(gfx_pl, gfx_pl_layout, ds_sets[DS_SST]);
            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);
            if (get_mvp_float16(&ubo.data.mvp)) memcpy(ubo.buff.mapped, &ubo.data, ubo.buff.size);
            else return 1;
        end_mode_3d();
        end_drawing();
    }

    wait_idle();
    free(pc.items);
    free(frame.data);
    vk_buff_destroy(pc.buff);
    vk_buff_destroy(frame.buff);
    vk_buff_destroy(ubo.buff);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(cs_render_ds_layout);
    vk_destroy_ds_layout(cs_resolve_ds_layout);
    vk_destroy_ds_layout(gfx_ds_layout);
    vk_destroy_pl_res(cs_render_pl, cs_render_pl_layout);
    vk_destroy_pl_res(cs_resolve_pl, cs_resolve_pl_layout);
    vk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    vk_unload_texture2(storage_tex);
    close_window();
    return 0;
}
