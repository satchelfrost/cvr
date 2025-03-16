#include "cvr.h"
#include <pthread.h>

#define PL_MPEG_IMPLEMENTATION
#include "ext/pl_mpeg.h"

#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../nob.h"

#define MAX_FPS_REC 100
#define WORK_GROUP_SZ 1024  // Workgroup size for render.comp
#define NUM_BATCHES 4       // Number of batches to dispatch
#define NUM_CCTVS 4
#define LOD_COUNT 7

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
    const char *name;
    VkDescriptorSet set;
    VkDescriptorSet depth_ds;
} Point_Cloud_Layer;

Point_Cloud_Layer pc_layers[LOD_COUNT] = {
    {.name = "res/arena_5060224_f32.vtx"},
    {.name = "res/arena_10120447_f32.vtx"},
    {.name = "res/arena_20240893_f32.vtx"},
    {.name = "res/arena_38795045_f32.vtx"},
    {.name = "res/arena_79276830_f32.vtx"},
    {.name = "res/arena_156866918_f32.vtx"},
    {.name = "res/arena_195661963_f32.vtx"},
};

bool load_points(const char *file, Point_Cloud_Layer *pc);
bool build_compute_cmds(size_t highest_lod);
bool update_render_ds_sets(Vk_Buffer ubo, Vk_Buffer depth_ubo, Vk_Buffer frame_buff, size_t lod);

typedef struct {
    Vk_Buffer **items;
    size_t count;
    size_t capacity;
} Vk_Buffer_Queue;

static Vk_Buffer_Queue vk_buff_queue = {0};

/* unity build includes */
#include "cmd_and_log.c"
#include "input.c"
#include "video.c"

typedef struct {
    Vk_Buffer buff;
    void *data;
} Frame_Buffer;

typedef struct {
    Vk_Buffer buff;
    void *data;
} Depth_Buffer;

Depth_Buffer depths = {0};

typedef struct {
    float16 main_cam_mvp;
    float16 cctv_mvps[NUM_CCTVS];
    Vector4 cctv_pos[NUM_CCTVS];
    Vector4 main_cam_pos;
    float16 model;
    int shader_mode;
    int cam_order_0;
    int cam_order_1;
    int cam_order_2;
    int cam_order_3;
    float blend_ratio;
    int frame_width;
    int frame_height;
    int elevation_based_occlusion;
    Vector2 frame_sizes[NUM_CCTVS];
} UBO_Data;

typedef struct {
    Vk_Buffer buff;
    UBO_Data data;
} Point_Cloud_UBO;

typedef struct {
    float16 cctv_mvps[NUM_CCTVS];
    Vector2 frame_sizes[NUM_CCTVS];
} Depth_Map_Data;

typedef struct {
    Vk_Buffer buff;
    Depth_Map_Data data;
} Depth_Map_UBO;

typedef struct {
    VkPipeline pl;
    VkPipelineLayout pl_layout;
    VkDescriptorSetLayout ds_layout;
    VkDescriptorSet ds;
} Pipeline;

/* pipelines */
static Pipeline cs_depth_map = {0}; // compute shader depth map
static Pipeline cs_render    = {0}; // compute shader render pipeline
static Pipeline cs_resolve   = {0}; // compute shader resolve (does the imageStore)
static Pipeline gfx_sst      = {0}; // graphics pass to screen space triangle
static VkDescriptorPool pool;

/* we store the initial window size so we don't have to deal with resizing the frame buffer (compute buffer),
 * as a consequence, resizing the window means we are either over/under-resolving */
static Window_Size initial_win_size = {0};

typedef struct {
    int idx;
    float dist_sqr;
} Distance_Sqr_Idx;

int dist_sqr_compare(const void *d1, const void *d2)
{
    if (((Distance_Sqr_Idx*)d1)->dist_sqr < ((Distance_Sqr_Idx*)d2)->dist_sqr)
        return -1;
    else if (((Distance_Sqr_Idx*)d2)->dist_sqr < ((Distance_Sqr_Idx*)d1)->dist_sqr)
        return 1;
    else
        return 0;
}

/* returns the camera indices in distance sorted order */
void get_cam_order(const Camera *cameras, size_t count, int *cam_order, size_t cam_order_count)
{
    Vector3 main_cam_pos = cameras[0].position;
    Distance_Sqr_Idx sqr_distances[4] = {0};
    for (size_t i = 1; i < count; i++) {
        Vector3 cctv_pos = cameras[i].position;
        sqr_distances[i - 1].dist_sqr = Vector3DistanceSqr(main_cam_pos, cctv_pos);
        sqr_distances[i - 1].idx = i - 1;
    }

    qsort(sqr_distances, VK_ARRAY_LEN(sqr_distances), sizeof(Distance_Sqr_Idx), dist_sqr_compare);
    for (size_t i = 0; i < cam_order_count; i++)
        cam_order[i] = sqr_distances[i].idx;
}

float calc_blend_ratio(const Camera *cameras, const int *cam_order)
{
    Vector3 main_cam_pos = cameras[0].position;
    Vector3 closest_cctv = cameras[cam_order[0] + 1].position;
    Vector3 close_cctv   = cameras[cam_order[1] + 1].position;
    Vector3 a = Vector3Subtract(main_cam_pos, closest_cctv);
    Vector3 b = Vector3Subtract(close_cctv, closest_cctv);

    /* normalized scalar projection of a onto b */
    return Vector3DotProduct(a, b) / Vector3LengthSqr(b);
}

#define read_attr(attr, sv)                   \
    do {                                      \
        memcpy(&attr, sv.data, sizeof(attr)); \
        sv.data  += sizeof(attr);             \
        sv.count -= sizeof(attr);             \
    } while(0)

bool load_points(const char *file, Point_Cloud_Layer *pc)
{
    bool result = true;

    /* reset count to zero in case we are loading a new point cloud */
    pc->count = 0;

    vk_log(VK_INFO, "reading vtx file %s", file);
    String_Builder sb = {0};
    if (!read_entire_file(file, &sb)) return_defer(false);

    String_View sv = sv_from_parts(sb.items, sb.count);
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
        da_append(pc, vert);
    }
    pc->buff.count = vtx_count;
    pc->buff.size = sizeof(Point_Vert) * vtx_count;

defer:
    sb_free(sb);
    return result;
}

Frame_Buffer alloc_frame_buff(size_t width, size_t height)
{
    size_t count = width * height;
    size_t sz = count * sizeof(uint64_t);
    Frame_Buffer frame = {
        .buff = {
            .count = count,
            .size  = sz,
        },
        .data = malloc(sz),
    };
    return frame;
}

void alloc_depth_buffs()
{
    size_t count = 0;
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        plm_frame_t frm = video_textures.initial_frames[i];
        count += frm.width * frm.height;
    }
    size_t sz = sizeof(uint32_t) * count;

    /* note the "depth" buffer is actually multiple (4) */
    depths = (Depth_Buffer) {
        .buff = {
            .count = count,
            .size  = sz,
        },
        .data = malloc(sz),
    };

    /* initialize to max depth */
    for (size_t i = 0; i < count; i++)
        ((uint32_t *)depths.data)[i] = 0xffffffff;
}

bool setup_ds_layouts()
{
    /* render.comp shader layout */
    VkDescriptorSetLayoutBinding cs_render_bindings[] = {
        {DS_BINDING(0,  UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1,  STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2,  STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(3,  STORAGE_BUFFER, COMPUTE_BIT)},
        {
            .binding = 4,
            .descriptorCount = VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,

        }
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = VK_ARRAY_LEN(cs_render_bindings),
        .pBindings = cs_render_bindings,
    };
    if (!vk_create_ds_layout(layout_ci, &cs_render.ds_layout)) return false;

    /* resolve.comp shader layout */
    VkDescriptorSetLayoutBinding cs_resolve_bindings[] = {
        {DS_BINDING(0, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1,  STORAGE_IMAGE, COMPUTE_BIT)},
    };
    layout_ci.bindingCount = VK_ARRAY_LEN(cs_resolve_bindings);
    layout_ci.pBindings = cs_resolve_bindings;
    if (!vk_create_ds_layout(layout_ci, &cs_resolve.ds_layout)) return false;

    /* texture sampler in fragment shader */
    VkDescriptorSetLayoutBinding gfx_binding = {
        DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)
    };
    layout_ci.bindingCount = 1;
    layout_ci.pBindings = &gfx_binding;
    if (!vk_create_ds_layout(layout_ci, &gfx_sst.ds_layout)) return false;

    /* depth_maps.comp */
    VkDescriptorSetLayoutBinding cs_depth_map_bindings[] = {
        {DS_BINDING(0,  UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1,  STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2,  STORAGE_BUFFER, COMPUTE_BIT)},
    };
    layout_ci.bindingCount = VK_ARRAY_LEN(cs_depth_map_bindings);
    layout_ci.pBindings = cs_depth_map_bindings;
    if (!vk_create_ds_layout(layout_ci, &cs_depth_map.ds_layout)) return false;

    return true;
}

bool setup_ds_pool()
{
     /* 1 in sst.frag + 12 in render.comp * LOD_COUNT */
    size_t sampler_count = 1 + VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT * LOD_COUNT;

    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 2)},                     // 1 in render.comp + 1 in depth_map.comp
        {DS_POOL(STORAGE_BUFFER, 6 * LOD_COUNT)},         // (3 in render.comp + 1 in resolve.comp + 2 in depth_map.comp) * LOD_COUNT
        {DS_POOL(COMBINED_IMAGE_SAMPLER, sampler_count)}, // 1 in default.frag
        {DS_POOL(STORAGE_IMAGE, 1)},                      // 1 in resolve.comp
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = VK_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 3 + LOD_COUNT * 2, // TODO: I feel like it should only need 2 + LOD_COUNT * 2...
    };

    if (!vk_create_ds_pool(pool_ci, &pool)) return false;

    return true;
}

bool setup_ds_sets(Vk_Buffer frame_buff, Vk_Texture storage_tex)
{
    /* allocate descriptor sets based on layouts for compute shader resolve and graphics screen space triangle */
    VkDescriptorSetAllocateInfo resolve_alloc = {DS_ALLOC(&cs_resolve.ds_layout, 1, pool)};
    if (!vk_alloc_ds(resolve_alloc, &cs_resolve.ds)) return false;
    VkDescriptorSetAllocateInfo gfx_alloc = {DS_ALLOC(&gfx_sst.ds_layout, 1, pool)};
    if (!vk_alloc_ds(gfx_alloc, &gfx_sst.ds)) return false;

    /* update descriptor sets */
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
        /* resolve.comp */
        {DS_WRITE_BUFF(0, STORAGE_BUFFER, cs_resolve.ds, &frame_buff_info)},
        {DS_WRITE_IMG (1, STORAGE_IMAGE,  cs_resolve.ds, &img_info)},
        /* default.frag */
        {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, gfx_sst.ds, &img_info)},
    };
    vk_update_ds(VK_ARRAY_LEN(writes), writes);

    return true;
}

bool update_render_ds_sets(Vk_Buffer ubo, Vk_Buffer depth_ubo, Vk_Buffer frame_buff, size_t lod)
{
    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = ubo.handle,
        .range  = ubo.size,
    };
    VkDescriptorBufferInfo depth_ubo_info = {
        .buffer = depth_ubo.handle,
        .range  = depth_ubo.size,
    };
    VkDescriptorBufferInfo pc_info = {
        .buffer = pc_layers[lod].buff.handle,
        .range  = pc_layers[lod].buff.size,
    };
    VkDescriptorBufferInfo frame_buff_info = {
        .buffer = frame_buff.handle,
        .range  = frame_buff.size,
    };
    VkDescriptorBufferInfo depth_buff_info = {
        .buffer = depths.buff.handle,
        .range  = depths.buff.size,
    };
    VkDescriptorImageInfo img_infos[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT] = {0};
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        for (size_t j = 0; j < VIDEO_PLANE_COUNT; j++) {
            img_infos[j + i * VIDEO_PLANE_COUNT].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            img_infos[j + i * VIDEO_PLANE_COUNT].imageView   = video_textures.planes[j + i * VIDEO_PLANE_COUNT].view;
            img_infos[j + i * VIDEO_PLANE_COUNT].sampler     = video_textures.planes[j + i * VIDEO_PLANE_COUNT].sampler;
        }
    }
    VkWriteDescriptorSet writes[] = {
        /* render.comp */
        {DS_WRITE_BUFF(0,  UNIFORM_BUFFER, pc_layers[lod].set, &ubo_info)},
        {DS_WRITE_BUFF(1,  STORAGE_BUFFER, pc_layers[lod].set, &pc_info)},
        {DS_WRITE_BUFF(2,  STORAGE_BUFFER, pc_layers[lod].set, &frame_buff_info)},
        {DS_WRITE_BUFF(3,  STORAGE_BUFFER, pc_layers[lod].set, &depth_buff_info)},
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = pc_layers[lod].set,
            .dstBinding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT,
            .pImageInfo = img_infos,
        },
        /* depth_map.comp */
        {DS_WRITE_BUFF(0,  UNIFORM_BUFFER, pc_layers[lod].depth_ds, &depth_ubo_info)},
        {DS_WRITE_BUFF(1,  STORAGE_BUFFER, pc_layers[lod].depth_ds, &pc_info)},
        {DS_WRITE_BUFF(2,  STORAGE_BUFFER, pc_layers[lod].depth_ds, &depth_buff_info)},
    };
    vk_update_ds(VK_ARRAY_LEN(writes), writes);

    return true;
}

typedef struct {
    uint32_t offset;
    uint32_t count;
} Push_Const;

bool build_compute_cmds(size_t highest_lod)
{
    size_t group_x = 1, group_y = 1, group_z = 1;
    if (!vk_rec_compute()) return false;

        /* loop through the lod layers of the point cloud (render.comp shader) */
        for (size_t lod = 0; lod <= highest_lod; lod++) {
            /* submit batches of points to render compute shader */
            group_x = ceil((float)pc_layers[lod].count / WORK_GROUP_SZ);
            size_t batch_size = ceil((float)group_x / NUM_BATCHES);
            for (size_t batch = 0; batch < NUM_BATCHES; batch++) {
                Push_Const pk = {
                    .offset = batch * batch_size * WORK_GROUP_SZ,
                    .count = pc_layers[lod].count,
                };
                vk_push_const(cs_render.pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(Push_Const), &pk);
                vk_compute(cs_render.pl, cs_render.pl_layout, pc_layers[lod].set, batch_size, group_y, group_z);
            }
        }

        /* protect against read after write hazards on frame buffer */
        vk_compute_pl_barrier();

        /* resolve the frame buffer (resolve.comp shader) */
        group_x = ceil(initial_win_size.width  / 16.0f);
        group_y = ceil(initial_win_size.height / 16.0f);
        vk_compute(cs_resolve.pl, cs_resolve.pl_layout, cs_resolve.ds, group_x, group_y, group_z);

    if (!vk_end_rec_compute()) return false;
    return true;
}

bool build_depth_compute_cmds(size_t highest_lod)
{
    size_t group_x = 1, group_y = 1, group_z = 1;
    if (!vk_rec_compute()) return false;
        /* loop through the lod layers of the point cloud (render.comp shader) */
        for (size_t lod = 0; lod <= highest_lod; lod++) {
            /* submit batches of points to render compute shader */
            group_x = ceil((float)pc_layers[lod].count / WORK_GROUP_SZ);
            size_t batch_size = ceil((float)group_x / NUM_BATCHES);
            for (size_t batch = 0; batch < NUM_BATCHES; batch++) {
                Push_Const pk = {
                    .offset = batch * batch_size * WORK_GROUP_SZ,
                    .count = pc_layers[lod].count,
                };
                vk_push_const(cs_depth_map.pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(Push_Const), &pk);
                vk_compute(cs_depth_map.pl, cs_depth_map.pl_layout, pc_layers[lod].depth_ds, batch_size, group_y, group_z);
            }
        }
    if (!vk_end_rec_compute()) return false;
    return true;
}

bool create_pipelines()
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 2 * sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &cs_render.ds_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };

    /* compute shader render pipeline */
    if (!vk_pl_layout_init(layout_ci, &cs_render.pl_layout))                                   return false;
    if (!vk_compute_pl_init("./res/render.comp.glsl.spv", cs_render.pl_layout, &cs_render.pl)) return false;

    /* compute shader render pipeline */
    layout_ci.pSetLayouts = &cs_depth_map.ds_layout;
    if (!vk_pl_layout_init(layout_ci, &cs_depth_map.pl_layout))                                         return false;
    if (!vk_compute_pl_init("./res/depth_map.comp.glsl.spv", cs_depth_map.pl_layout, &cs_depth_map.pl)) return false;

    /* compute shader resolve pipeline */
    layout_ci.pSetLayouts = &cs_resolve.ds_layout;
    layout_ci.pushConstantRangeCount = 0;
    if (!vk_pl_layout_init(layout_ci, &cs_resolve.pl_layout))                                     return false;
    if (!vk_compute_pl_init("./res/resolve.comp.glsl.spv", cs_resolve.pl_layout, &cs_resolve.pl)) return false;

    /* screen space triangle + frag image sampler for raster display */
    layout_ci.pSetLayouts = &gfx_sst.ds_layout;
    if (!vk_pl_layout_init(layout_ci, &gfx_sst.pl_layout)) return false;
    if (!vk_sst_pl_init(gfx_sst.pl_layout, &gfx_sst.pl))   return false;

    return true;
}

bool update_pc_ubo(Camera *four_cameras, int shader_mode, int *cam_order, float blend_ratio,
                   Point_Cloud_UBO *ubo, bool elevation_based_occlusion)
{
    /* update mvp for main viewing camera */
    if (!get_mvp_float16(&ubo->data.main_cam_mvp)) return false;

    /* calculate mvps for cctvs */
    Matrix model = {0};
    if (!get_matrix_tos(&model)) return false;
    for (size_t i = 0; i < 4; i++) {
        Matrix view = MatrixLookAt(
            four_cameras[i].position,
            four_cameras[i].target,
            four_cameras[i].up
        );
        Matrix proj = get_proj_aspect(four_cameras[i], video_textures.aspects[i]);
        Matrix view_proj = MatrixMultiply(view, proj);
        Matrix mvp = MatrixMultiply(model, view_proj);
        ubo->data.cctv_mvps[i] = MatrixToFloatV(mvp);
        ubo->data.cctv_pos[i] = (Vector4) {
            four_cameras[i].position.x,
            four_cameras[i].position.y,
            four_cameras[i].position.z,
            1.0,
        };
        plm_frame_t frm = video_textures.initial_frames[i];
        ubo->data.frame_sizes[i] = (Vector2) {.x = frm.width, .y = frm.height};
    }

    ubo->data.model = MatrixToFloatV(model);
    ubo->data.cam_order_0 = cam_order[0];
    ubo->data.cam_order_1 = cam_order[1];
    ubo->data.cam_order_2 = cam_order[2];
    ubo->data.cam_order_3 = cam_order[3];
    Camera *main_cam = four_cameras - 1;
    ubo->data.main_cam_pos = (Vector4) {
        main_cam[0].position.x,
        main_cam[0].position.y,
        main_cam[0].position.z,
        1.0,
    };
    ubo->data.shader_mode = shader_mode;
    ubo->data.elevation_based_occlusion = (elevation_based_occlusion) ? 1 : 0;
    ubo->data.blend_ratio = blend_ratio;
    ubo->data.frame_width  = initial_win_size.width;
    ubo->data.frame_height = initial_win_size.height;

    memcpy(ubo->buff.mapped, &ubo->data, ubo->buff.size);

    return true;
}

bool update_depth_map_ubo(Camera *four_cameras, Depth_Map_UBO *ubo)
{
    /* calculate mvps for cctvs */
    Matrix model = {0};
    if (!get_matrix_tos(&model)) return false;
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        Matrix view = MatrixLookAt(
            four_cameras[i].position,
            four_cameras[i].target,
            four_cameras[i].up
        );
        Matrix proj = get_proj_aspect(four_cameras[i], video_textures.aspects[i]);
        Matrix view_proj = MatrixMultiply(view, proj);
        Matrix mvp = MatrixMultiply(model, view_proj);
        ubo->data.cctv_mvps[i] = MatrixToFloatV(mvp);

        plm_frame_t frm = video_textures.initial_frames[i];
        ubo->data.frame_sizes[i] = (Vector2) {.x = frm.width, .y = frm.height};
    }
    memcpy(ubo->buff.mapped, &ubo->data, ubo->buff.size);
    return true;
}

Camera cameras[] = {
    { // Camera to rule all cameras
        .position   = {38.54, 23.47, 42.09},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {25.18, 16.37, 38.97},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // cctv 1 - suite e
        .position   = {35.41, 18.88, 40.50},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {30.62, 17.51, 39.46},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // cctv 2 - suite w
        .position   = {-64.66, 18.69, 21.02},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {-55.18, 15.71, 22.67},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // cctv 3 - suite se
        .position   = {15.92, 20.52, 69.19},
        .up         = {0.02, 1.00, -0.08},
        .target     = {-3.21, 13.57, 65.88},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // cctv 4 - suite nw
        .position   = {-43.64, 15.77, -4.89},
        .up         = {-0.01, 1.00, 0.09},
        .target     = {-22.14, 9.92, -2.76},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
};

int main(int argc, char **argv)
{
    Point_Cloud_UBO ubo = {.buff = {.count = 1, .size = sizeof(UBO_Data)}};
    Depth_Map_UBO depth_map_ubo = {.buff = {.count = 1, .size = sizeof(Depth_Map_Data)}};

    /* initialize window and Vulkan */
    Config config = {0};
    if (!handle_usr_cmds(argc, argv, &config)) {
        log_usage(config.program);
        return 1;
    }
    size_t lod = config.highest_lod;
    for (size_t i = 0; i <= lod; i++)
        if (!load_points(pc_layers[i].name, &pc_layers[i])) return 1;
    log_point_count(pc_layers, lod);

    if (config.fullscreen) {
        enable_full_screen();
        init_window(0, 0, "Arena Point Cloud Rasterization");
    } else {
        init_window(config.width, config.height, "Arena Point Cloud Rasterization");
        set_window_pos(config.x_pos, config.y_pos);
    }

    /* load videos, and decode one initial frame from each video */
    if (!load_videos()) return 1;
    if (!decode_initial_vid_frames()) return 1;

    /* allocate frame buffers */
    initial_win_size = get_window_size();
    Vk_Texture storage_tex = {.img.extent = {initial_win_size.width,  initial_win_size.height}};
    Frame_Buffer frame = alloc_frame_buff(initial_win_size.width, initial_win_size.height);
    alloc_depth_buffs();

    /* general state tracking */
    save_camera_defaults(&cameras[1]);
    int cam_order[4] = {0};
    bool elevation_based_occlusion = false;
    // bool depth_map_needs_update = true;

    for (size_t i = 0; i <= lod; i++) {
        if (!vk_comp_buff_staged_upload(&pc_layers[i].buff, pc_layers[i].items))
            return 1;
        da_append(&vk_buff_queue, &pc_layers[i].buff);
    }
    if (!vk_comp_buff_staged_upload(&frame.buff, frame.data))   return 1;
    if (!vk_comp_buff_staged_upload(&depths.buff, depths.data)) return 1;
    if (!vk_ubo_init(&ubo.buff))                                return 1;
    if (!vk_ubo_init(&depth_map_ubo.buff))                      return 1;
    if (!vk_create_storage_img(&storage_tex))                   return 1;
    da_append(&vk_buff_queue, &frame.buff);
    da_append(&vk_buff_queue, &depths.buff);
    da_append(&vk_buff_queue, &ubo.buff);
    da_append(&vk_buff_queue, &depth_map_ubo.buff);

    /* setup descriptors */
    if (!setup_ds_layouts())                     return 1;
    if (!setup_ds_pool())                        return 1;
    if (!setup_ds_sets(frame.buff, storage_tex)) return 1;

    /* launch a separate thread to start decoding video */
    video_queue_init();

    /* allocate descriptor sets based on layouts */
    for (size_t i = 0; i < LOD_COUNT; i++) {
        VkDescriptorSetAllocateInfo render_alloc = {DS_ALLOC(&cs_render.ds_layout, 1, pool)};
        if (!vk_alloc_ds(render_alloc, &pc_layers[i].set)) return false;
        VkDescriptorSetAllocateInfo depth_alloc = {DS_ALLOC(&cs_depth_map.ds_layout, 1, pool)};
        if (!vk_alloc_ds(depth_alloc, &pc_layers[i].depth_ds)) return false;
    }

    for (size_t i = 0; i <= lod; i++)
        if (!update_render_ds_sets(ubo.buff, depth_map_ubo.buff, frame.buff, i)) return 1;

    /* create pipelines */
    if (!create_pipelines()) return 1;

    /* do an intial render pass with the depth buffer */
    if (!build_depth_compute_cmds(lod)) return 1;
    begin_mode_3d(cameras[cam_view_idx]);
        translate(0.0f, 0.0f, -100.0f);
        rotate_x(-PI / 2);
        if (!update_depth_map_ubo(&cameras[1], &depth_map_ubo)) return 1;
        if (!vk_compute_fence_wait()) return 1;
        if (!vk_submit_compute()) return 1;
    end_mode_3d();
    wait_idle();

    /* record commands for compute buffer */
    if (!build_compute_cmds(lod)) return 1;

    /* game loop */
    while (!window_should_close()) {
        if (!handle_input(cameras, VK_ARRAY_LEN(cameras), &playing, &lod, ubo.buff, depth_map_ubo.buff, frame.buff))
            return 1;
        if (records.collecting) record_data();
        if (!try_video_dequeue()) return 1;

        /* compute shader submission */
        begin_mode_3d(cameras[cam_view_idx]);
            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);
            get_cam_order(cameras, VK_ARRAY_LEN(cameras), cam_order, VK_ARRAY_LEN(cam_order));
            float blend_ratio = calc_blend_ratio(cameras, cam_order);
            if (!vk_compute_fence_wait()) return 1;
            elevation_based_occlusion = (cameras[cam_view_idx].position.y < -1.5f) ? true : false;
            if (!update_pc_ubo(&cameras[1], shader_mode, cam_order, blend_ratio, &ubo, elevation_based_occlusion)) return 1;
            if (!vk_submit_compute()) return 1;
        end_mode_3d();

        /* draw command for screen space triangle (sst) */
        begin_timer();
        if (!vk_wait_to_begin_gfx()) return 1;
            vk_begin_rec_gfx();
            vk_raster_sampler_barrier(storage_tex.img.handle);
            vk_begin_render_pass(0.0f, 0.0f, 0.0f, 1.0f);
            vk_draw_sst(gfx_sst.pl, gfx_sst.pl_layout, gfx_sst.ds);
        end_drawing(); // ends gfx recording, renderpass, and submits
    }

    /* stop the video decoder, also waits for launched thread to stop */
    video_queue_destroy();

    wait_idle();

    // TODO: at some point switch to arenas
    free(frame.data);
    for (size_t i = 0; i < LOD_COUNT; i++)
        da_free(pc_layers[i]);
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        plm_destroy(video_textures.plms[i]);
        free(video_textures.initial_frames[i].y.data);
        free(video_textures.initial_frames[i].cb.data);
        free(video_textures.initial_frames[i].cr.data);
        for (size_t j = 0; j < VIDEO_PLANE_COUNT; j++) {
            vk_buff_destroy(&video_textures.stg_buffs[j + i * VIDEO_PLANE_COUNT]);
            vk_unload_texture(&video_textures.planes[j + i * VIDEO_PLANE_COUNT]);
        }
    }

    /* cleanup vulkan resources */
    for (size_t i = 0; i < vk_buff_queue.count; i++)
        vk_buff_destroy(vk_buff_queue.items[i]);
    da_free(vk_buff_queue);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(cs_render.ds_layout);
    vk_destroy_ds_layout(cs_depth_map.ds_layout);
    vk_destroy_ds_layout(cs_resolve.ds_layout);
    vk_destroy_ds_layout(gfx_sst.ds_layout);
    vk_destroy_pl_res(cs_render.pl, cs_render.pl_layout);
    vk_destroy_pl_res(cs_depth_map.pl, cs_depth_map.pl_layout);
    vk_destroy_pl_res(cs_resolve.pl, cs_resolve.pl_layout);
    vk_destroy_pl_res(gfx_sst.pl, gfx_sst.pl_layout);
    vk_unload_texture(&storage_tex);
    close_window();
    return 0;
}
