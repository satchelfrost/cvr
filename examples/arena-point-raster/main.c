#include "cvr.h"
#include "vk_ctx.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>
#include <float.h>
#include <pthread.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#define MAX_FPS_REC 100
#define SUBGROUP_SZ 1024    // Subgroup size for render.comp
#define NUM_BATCHES 4       // Number of batches to dispatch
#define MAX_LOD 7
#define NUM_CCTVS 4
// #define DEBUG_QUEUE_PRINT // disabling doesn't print the video queue

/* which videos to use for texture sampling */
// #define HIGH_RES
#define MEDIUM_RES

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
    const char *name;
    VkDescriptorSet set;
} Point_Cloud_Layer;

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
    Vk_Buffer buff;
    void *data;
} Texture_Buffer;

Texture_Buffer tex_buff = {0};

typedef struct {
    float16 main_cam_mvp;
    float16 cctv_mvps[NUM_CCTVS];
    Vector4 cctv_pos[NUM_CCTVS];
    float16 model;
    int shader_mode;
    int cam_order_0;
    int cam_order_1;
    int cam_order_2;
    int cam_order_3;
    float blend_ratio;
    int frame_width;
    int frame_height;
} UBO_Data;

typedef struct {
    Vk_Buffer buff;
    UBO_Data data;
} Point_Cloud_UBO;

typedef enum {
    VIDEO_PLANE_Y,
    VIDEO_PLANE_CB,
    VIDEO_PLANE_CR,
    VIDEO_PLANE_COUNT,
} Video_Plane_Type;

typedef enum {
    VIDEO_IDX_SUITE_E,
    VIDEO_IDX_SUITE_W,
    VIDEO_IDX_SUITE_NW,
    VIDEO_IDX_SUITE_SE,
    VIDEO_IDX_COUNT,
} Video_Idx;


const char *video_names[] = {
#if defined(HIGH_RES)
    "suite_e_2560x1920",
    "suite_w_2560x1920",
    "suite_se_2560x1440",
    "suite_nw_2560x1440",
#elif defined(MEDIUM_RES)
    "suite_e_1280x960",
    "suite_w_1280x960",
    "suite_se_1280x720",
    "suite_nw_1280x720",
#else
    "suite_e_960x720",
    "suite_w_960x720",
    "suite_se_960x540",
    "suite_nw_960x540",
#endif
};

typedef struct {
    Vk_Texture planes[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    Vk_Buffer stg_buffs[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    plm_t *plms[VIDEO_IDX_COUNT];
    float aspects[VIDEO_IDX_COUNT];
    plm_frame_t initial_frames[VIDEO_IDX_COUNT];
} Video_Textures;

Video_Textures video_textures = {0};

#define MAX_QUEUED_FRAMES 20
typedef struct {
    plm_frame_t frames[VIDEO_IDX_COUNT * MAX_QUEUED_FRAMES];
    int head;
    int tail;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} Video_Queue;

Video_Queue video_queue = {0};

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
VkDescriptorSet cs_resolve_ds;
VkDescriptorSet gfx_sst; // screen space triangle

Point_Cloud_Layer pc_layers[MAX_LOD] = {
    {.name = "res/arena_5060224_f32.vtx"},
    {.name = "res/arena_10120447_f32.vtx"},
    {.name = "res/arena_20240893_f32.vtx"},
    {.name = "res/arena_38795045_f32.vtx"},
    {.name = "res/arena_79276830_f32.vtx"},
    {.name = "res/arena_156866918_f32.vtx"},
    {.name = "res/arena_195661963_f32.vtx"},
};

typedef enum {DS_RESOLVE, DS_SST, DS_COUNT} DS_SET;
VkDescriptorSet ds_sets[DS_COUNT] = {0};

bool update_video_texture(void *data, Video_Idx vid_idx, Video_Plane_Type vid_plane_type);

void video_queue_init()
{
    pthread_mutex_init(&video_queue.mutex, NULL);
    pthread_cond_init(&video_queue.not_full, NULL);
    pthread_cond_init(&video_queue.not_empty, NULL);

    /* use a initial frame to determine memory requirements */
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        for (size_t j = 0; j < MAX_QUEUED_FRAMES; j++) {
            plm_frame_t *frame = &video_textures.initial_frames[i];
            video_queue.frames[i + j * VIDEO_IDX_COUNT] = *frame; // shallow copy first
            size_t y_size  = frame->y.width  * frame->y.height;
            size_t cb_size = frame->cb.width * frame->cb.height;
            size_t cr_size = frame->cr.width * frame->cr.height;
            video_queue.frames[i + j * VIDEO_IDX_COUNT].y.data  = malloc(y_size);
            video_queue.frames[i + j * VIDEO_IDX_COUNT].cb.data = malloc(cb_size);
            video_queue.frames[i + j * VIDEO_IDX_COUNT].cr.data = malloc(cr_size);
        }
    }
}

void video_queue_destroy()
{
    pthread_mutex_destroy(&video_queue.mutex);
    pthread_cond_destroy(&video_queue.not_full);
    pthread_cond_destroy(&video_queue.not_empty);

    for (size_t i = 0; i < MAX_QUEUED_FRAMES * VIDEO_IDX_COUNT; i++) {
        free(video_queue.frames[i].y.data);
        free(video_queue.frames[i].cb.data);
        free(video_queue.frames[i].cr.data);
    }
}

bool video_enqueue()
{
#ifdef DEBUG_QUEUE_PRINT
    char queue_str[MAX_QUEUED_FRAMES + 3];
    queue_str[0] = '[';
    queue_str[MAX_QUEUED_FRAMES + 1] = ']';
    queue_str[MAX_QUEUED_FRAMES + 2] = 0;
    for (int i = 0; i < MAX_QUEUED_FRAMES; i++)
        queue_str[i + 1] = (i < video_queue.size) ? '*' : ' ';
    nob_log(NOB_INFO, "%s", queue_str);
#endif

    /* first decode the frames */
    plm_frame_t temp_frames[VIDEO_IDX_COUNT];
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        plm_frame_t *frame = plm_decode_video(video_textures.plms[i]);
        if (!frame) return false;

        size_t y_size  = frame->y.width  * frame->y.height;
        size_t cb_size = frame->cb.width * frame->cb.height;
        size_t cr_size = frame->cr.width * frame->cr.height;
        temp_frames[i] = *frame; // shallow copy first
        memcpy(temp_frames[i].y.data , frame->y.data,  y_size);
        memcpy(temp_frames[i].cb.data, frame->cb.data, cb_size);
        memcpy(temp_frames[i].cr.data, frame->cr.data, cr_size);
    }

    pthread_mutex_lock(&video_queue.mutex);
        /* if queue is full then wait for consumer */
        if (video_queue.size == MAX_QUEUED_FRAMES)
            pthread_cond_wait(&video_queue.not_full, &video_queue.mutex);

        /* decode the next "VIDEO_IDX_COUNT" video frames and enqueue */
        for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
            plm_frame_t *saved = &video_queue.frames[i + video_queue.head * VIDEO_IDX_COUNT];
            size_t y_size  = temp_frames[i].y.width  * temp_frames[i].y.height;
            size_t cb_size = temp_frames[i].cb.width * temp_frames[i].cb.height;
            size_t cr_size = temp_frames[i].cr.width * temp_frames[i].cr.height;
            memcpy(saved->y.data , temp_frames[i].y.data,  y_size);
            memcpy(saved->cb.data, temp_frames[i].cb.data, cb_size);
            memcpy(saved->cr.data, temp_frames[i].cr.data, cr_size);
        }
        video_queue.head = (video_queue.head + 1) % MAX_QUEUED_FRAMES;
        video_queue.size++;
        pthread_cond_signal(&video_queue.not_empty);
    pthread_mutex_unlock(&video_queue.mutex);

    return true;
}

bool video_dequeue()
{
    pthread_mutex_lock(&video_queue.mutex);
        /* if queue is empty then wait for producer */
        if (video_queue.size == 0)
            pthread_cond_wait(&video_queue.not_empty, &video_queue.mutex);

        /* dequeue the next four video frames */
        for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
            plm_frame_t *saved = &video_queue.frames[i + video_queue.tail * VIDEO_IDX_COUNT];
            if (!update_video_texture(saved->y.data, i, VIDEO_PLANE_Y))   return false;
            if (!update_video_texture(saved->cb.data, i, VIDEO_PLANE_CB)) return false;
            if (!update_video_texture(saved->cr.data, i, VIDEO_PLANE_CR)) return false;
        }
        video_queue.tail = (video_queue.tail + 1) % MAX_QUEUED_FRAMES;
        video_queue.size--;
        pthread_cond_signal(&video_queue.not_full);
    pthread_mutex_unlock(&video_queue.mutex);

    return true;
}

void *producer(void *arg)
{
    (void)arg;
    while (true) {
        if (!video_enqueue()) {
            /* loop all videos even if just one has finished */
            for (size_t i = 0; i < VIDEO_IDX_COUNT; i++)
                plm_rewind(video_textures.plms[i]);
        }
    };
    nob_log(NOB_INFO, "producer finished");
    return NULL;
}

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

    qsort(sqr_distances, NOB_ARRAY_LEN(sqr_distances), sizeof(Distance_Sqr_Idx), dist_sqr_compare);
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

void copy_camera_infos(Camera *dst, const Camera *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
}

typedef enum {
    SHADER_MODE_MODEL,
    SHADER_MODE_FRUSTUM_OVERLAP,
    SHADER_MODE_SINGLE_VID_TEX,
    SHADER_MODE_MULTI_VID_TEX,
    SHADER_MODE_MULTI_VID_TEX_BLEND,
    SHADER_MODE_VERT_COLORED_BY_CCTV,
    SHADER_MODE_VERT_CLOSEST_CCTV_TEX,
    SHADER_MODE_VERT_CLOSEST_CCTV_TEX_W_FALLBACK,
    SHADER_MODE_COUNT,
} Shader_Mode;

void log_shader_mode(Shader_Mode mode)
{
    switch (mode) {
    case SHADER_MODE_MODEL:
        nob_log(NOB_INFO, "Shader mode: just the point cloud model");
        break;
    case SHADER_MODE_FRUSTUM_OVERLAP:
        nob_log(NOB_INFO, "Shader mode: cctv frustum overlap view");
        break;
    case SHADER_MODE_SINGLE_VID_TEX:
        nob_log(NOB_INFO, "Shader mode: single video texture");
        break;
    case SHADER_MODE_MULTI_VID_TEX:
        nob_log(NOB_INFO, "Shader mode: multiple video textures");
        break;
    case SHADER_MODE_MULTI_VID_TEX_BLEND:
        nob_log(NOB_INFO, "Shader mode: multiple video textures with smooth blend");
        break;
    case SHADER_MODE_VERT_COLORED_BY_CCTV:
        nob_log(NOB_INFO, "Shader mode: vertex colored by closest cctv (darker regions cannot be seen by closest cctv)");
        break;
    case SHADER_MODE_VERT_CLOSEST_CCTV_TEX:
        nob_log(NOB_INFO, "Shader mode: vertex gets texture color from closest cctv");
        break;
    case SHADER_MODE_VERT_CLOSEST_CCTV_TEX_W_FALLBACK:
        nob_log(NOB_INFO, "Shader mode: vertex gets texture color from closest cctv, and fallback texture (next closest)");
        break;
    default:
        nob_log(NOB_ERROR, "Shader mode: unrecognized %d", mode);
        break;
    }
}

bool load_points(const char *file, Point_Cloud_Layer *pc)
{
    bool result = true;

    /* reset count to zero in case we are loading a new point cloud */
    pc->count = 0;

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
        nob_da_append(pc, vert);
    }

    pc->buff.count = vtx_count;
    pc->buff.size = sizeof(Point_Vert) * vtx_count;

defer:
    nob_sb_free(sb);
    return result;
}

Frame_Buffer alloc_frame_buff(size_t frame_buff_sz)
{
    Frame_Buffer frame = {0};
    frame.buff.count = frame_buff_sz;
    frame.buff.size  = sizeof(uint64_t) * frame.buff.count;
    frame.data = malloc(frame.buff.size);
    return frame;
}

bool setup_ds_layouts()
{
    /* render.comp shader layout */
    VkDescriptorSetLayoutBinding cs_render_bindings[] = {
        {DS_BINDING(0,  UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1,  STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2,  STORAGE_BUFFER, COMPUTE_BIT)},
        {
            .binding = 3,
            .descriptorCount = VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,

        }
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = NOB_ARRAY_LEN(cs_render_bindings),
        .pBindings = cs_render_bindings,
    };
    if (!vk_create_ds_layout(layout_ci, &cs_render_ds_layout)) return false;

    /* resolve.comp shader layout */
    VkDescriptorSetLayoutBinding cs_resolve_bindings[] = {
        {DS_BINDING(0, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1,  STORAGE_IMAGE, COMPUTE_BIT)},
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
     /* 1 in sst.frag + 12 in render.comp * MAX_LOD */
    size_t sampler_count = 1 + VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT * MAX_LOD;

    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 1)},                    // 1 in render.comp
        {DS_POOL(STORAGE_BUFFER, 3 * MAX_LOD)},          // (2 in render.comp + 1 in resolve.comp) * MAX_LOD
        {DS_POOL(COMBINED_IMAGE_SAMPLER, sampler_count)},
        {DS_POOL(STORAGE_IMAGE, 1)},                     // 1 in resolve.comp
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = NOB_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 2 + MAX_LOD, // compute shader resolve, graphics sst, compute shader render (x MAX_LOD)
    };

    if (!vk_create_ds_pool(pool_ci, &pool)) return false;

    return true;
}

bool setup_ds_sets(Vk_Buffer frame_buff, Vk_Texture storage_tex)
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetLayout layouts[] = {cs_resolve_ds_layout, gfx_ds_layout};
    size_t count = NOB_ARRAY_LEN(layouts);
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(layouts, count, pool)};
    if (!vk_alloc_ds(alloc, ds_sets)) return false;

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
        {DS_WRITE_BUFF(0, STORAGE_BUFFER, ds_sets[DS_RESOLVE], &frame_buff_info)},
        {DS_WRITE_IMG (1, STORAGE_IMAGE,  ds_sets[DS_RESOLVE], &img_info)},
        /* default.frag */
        {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, ds_sets[DS_SST], &img_info)},
    };
    vk_update_ds(NOB_ARRAY_LEN(writes), writes);

    return true;
}

bool update_render_ds_sets(Vk_Buffer ubo, Vk_Buffer frame_buff, size_t lod)
{
    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = ubo.handle,
        .range  = ubo.size,
    };
    VkDescriptorBufferInfo pc_info = {
        .buffer = pc_layers[lod].buff.handle,
        .range  = pc_layers[lod].buff.size,
    };
    VkDescriptorBufferInfo frame_buff_info = {
        .buffer = frame_buff.handle,
        .range  = frame_buff.size,
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
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = pc_layers[lod].set,
            .dstBinding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT,
            .pImageInfo = img_infos,
        },
    };
    vk_update_ds(NOB_ARRAY_LEN(writes), writes);

    return true;
}

bool init_video_texture(void *data, int width, int height, Video_Idx vid_idx, Video_Plane_Type vid_plane_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    stg_buff->size  = width * height * format_to_size(VK_FORMAT_R8_UNORM);
    stg_buff->count = width * height;
    if (!vk_stg_buff_init(stg_buff, data, true)) return false;

    /* create the image */
    Vk_Image vk_img = {
        .extent  = {width, height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8_UNORM,
    };
    result = vk_img_init(
        &vk_img,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (!result) return false;

    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    vk_img_copy(vk_img.handle, stg_buff->handle, vk_img.extent);
    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    /* create image view */
    VkImageView img_view;
    if (!vk_img_view_init(vk_img, &img_view)) {
        nob_log(NOB_ERROR, "failed to create image view");
        return false;
    };

    VkSampler sampler;
    if (!vk_sampler_init(&sampler)) return false;

    Vk_Texture plane = {
        .view = img_view,
        .sampler = sampler,
        .img = vk_img,
    };
    video_textures.planes[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT] = plane;

    return true;
}

bool update_video_texture(void *data, Video_Idx vid_idx, Video_Plane_Type vid_plane_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    memcpy(stg_buff->mapped, data, stg_buff->size);

    Vk_Image vk_img = video_textures.planes[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT].img;
    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    vk_img_copy(vk_img.handle, stg_buff->handle, vk_img.extent);
    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    return result;
}

bool build_compute_cmds(size_t highest_lod)
{
    size_t group_x = 1, group_y = 1, group_z = 1;
    if (!vk_rec_compute()) return false;
        /* loop through the lod layers of the point cloud (render.comp shader) */
        for (size_t lod = 0; lod <= highest_lod; lod++) {
            /* submit batches of points to render compute shader */
            group_x = pc_layers[lod].count / SUBGROUP_SZ + 1;
            size_t batch_size = group_x / NUM_BATCHES;
            for (size_t batch = 0; batch < NUM_BATCHES; batch++) {
                uint32_t offset = batch * batch_size * SUBGROUP_SZ;
                uint32_t count = pc_layers[lod].count;
                vk_push_const(cs_render_pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &offset);
                vk_push_const(cs_render_pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), sizeof(uint32_t), &count);
                vk_compute(cs_render_pl, cs_render_pl_layout, pc_layers[lod].set, batch_size, group_y, group_z);
            }
        }

        /* protect against read after write hazards on frame buffer */
        vk_compute_pl_barrier();

        /* resolve the frame buffer (resolve.comp shader) */
        Window_Size win_size = get_window_size();
        group_x = win_size.width / 16 + 1;
        group_y = win_size.height / 16 + 1;
        vk_compute(cs_resolve_pl, cs_resolve_pl_layout, ds_sets[DS_RESOLVE], group_x, group_y, group_z);

    if (!vk_end_rec_compute()) return false;
    return true;
}

bool create_pipelines()
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 2 * sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &cs_render_ds_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };

    /* compute shader render pipeline */
    if (!vk_pl_layout_init(layout_ci, &cs_render_pl_layout))                                 return false;
    if (!vk_compute_pl_init("./res/render.comp.spv", cs_render_pl_layout, &cs_render_pl))    return false;

    /* compute shader resolve pipeline */
    layout_ci.pSetLayouts = &cs_resolve_ds_layout;
    layout_ci.pushConstantRangeCount = 0;
    if (!vk_pl_layout_init(layout_ci, &cs_resolve_pl_layout))                                return false;
    if (!vk_compute_pl_init("./res/resolve.comp.spv", cs_resolve_pl_layout, &cs_resolve_pl)) return false;

    /* screen space triangle + frag image sampler for raster display */
    layout_ci.pSetLayouts = &gfx_ds_layout;
    if (!vk_pl_layout_init(layout_ci, &gfx_pl_layout))                                       return false;
    if (!vk_sst_pl_init(gfx_pl_layout, &gfx_pl))                                             return false;

    return true;
}

bool update_pc_ubo(Camera *four_cameras, int shader_mode, int *cam_order, float blend_ratio, Point_Cloud_UBO *ubo)
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
    }

    ubo->data.model = MatrixToFloatV(model);
    ubo->data.cam_order_0 = cam_order[0];
    ubo->data.cam_order_1 = cam_order[1];
    ubo->data.cam_order_2 = cam_order[2];
    ubo->data.cam_order_3 = cam_order[3];
    ubo->data.shader_mode = shader_mode;
    ubo->data.blend_ratio = blend_ratio;
    Window_Size win_size = get_window_size();
    ubo->data.frame_width = win_size.width;
    ubo->data.frame_height = win_size.height;

    memcpy(ubo->buff.mapped, &ubo->data, ubo->buff.size);

    return true;
}

void log_cameras(Camera *cameras, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        Vector3 pos = cameras[i].position;
        Vector3 up  = cameras[i].up;
        Vector3 tg  = cameras[i].target;
        float fov   = cameras[i].fovy;
        nob_log(NOB_INFO, "Camera %d", i);
        nob_log(NOB_INFO, "    position {%.2f, %.2f, %.2f}", pos.x, pos.y, pos.z);
        nob_log(NOB_INFO, "    up       {%.2f, %.2f, %.2f}", up.x, up.y, up.z);
        nob_log(NOB_INFO, "    target   {%.2f, %.2f, %.2f}", tg.x, tg.y, tg.z);
        nob_log(NOB_INFO, "    fovy     %.2f", fov);
    }
}

void log_controls()
{
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "| Keyboard |");
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "    ------------");
    nob_log(NOB_INFO, "    | Movement |");
    nob_log(NOB_INFO, "    ------------");
    nob_log(NOB_INFO, "        [W] - Forward");
    nob_log(NOB_INFO, "        [A] - Left");
    nob_log(NOB_INFO, "        [S] - Back");
    nob_log(NOB_INFO, "        [D] - Right");
    nob_log(NOB_INFO, "        [E] - Up");
    nob_log(NOB_INFO, "        [Q] - Down");
    nob_log(NOB_INFO, "        [Shift] - Fast movement");
    nob_log(NOB_INFO, "        Right Click + Mouse Movement = Rotation");
    nob_log(NOB_INFO, "    ------------");
    nob_log(NOB_INFO, "    | Hot keys |");
    nob_log(NOB_INFO, "    ------------");
    nob_log(NOB_INFO, "        [M] - Next shader mode");
    nob_log(NOB_INFO, "        [Shift + M] - Previous shader mode");
    nob_log(NOB_INFO, "        [C] - Change piloted camera");
    nob_log(NOB_INFO, "        [P] - Play/Pause");
    nob_log(NOB_INFO, "        [R] - Record FPS");
    nob_log(NOB_INFO, "        [P] - Print camera info");
    nob_log(NOB_INFO, "        [Space] - Reset cameras to default position");
    nob_log(NOB_INFO, "-----------");
    nob_log(NOB_INFO, "| Gamepad |");
    nob_log(NOB_INFO, "-----------");
    nob_log(NOB_INFO, "    ------------");
    nob_log(NOB_INFO, "    | Movement |");
    nob_log(NOB_INFO, "    ------------");
    nob_log(NOB_INFO, "        [Left Analog] - Translation");
    nob_log(NOB_INFO, "        [Right Analog] - Rotation");
    nob_log(NOB_INFO, "    ---------");
    nob_log(NOB_INFO, "    | Other |");
    nob_log(NOB_INFO, "    ---------");
    nob_log(NOB_INFO, "        [Right Trigger] - Next shader mode");
    nob_log(NOB_INFO, "        [Left Trigger] - Previous shader mode");
    nob_log(NOB_INFO, "        [Right Face Right Button] - Play/Pause");
    nob_log(NOB_INFO, "        [Right Face Down Button] - Record FPS");
    nob_log(NOB_INFO, "        [D Pad Left] - decrease point cloud LOD");
    nob_log(NOB_INFO, "        [D Pad Right] - increase point cloud LOD");
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

Camera camera_defaults[4] = {0};

int main(int argc, char **argv)
{
    Point_Cloud_UBO ubo = {.buff = {.count = 1, .size = sizeof(UBO_Data)}};
    FPS_Record record = {.max = MAX_FPS_REC};
    size_t lod = 0;
    if (!load_points(pc_layers[lod].name, &pc_layers[lod])) return 1;

    /* initialize window and Vulkan */
    if (argc > 4) {
        init_window(atoi(argv[3]), atoi(argv[4]), "point cloud");
        set_window_pos(atoi(argv[1]), atoi(argv[2]));
    } else {
        init_window(1600, 900, "arena point cloud rasterization");
    }

    Window_Size win_size = get_window_size();
    Vk_Texture storage_tex = {.img.extent = {win_size.width, win_size.height}};
    Frame_Buffer frame_buff = alloc_frame_buff(win_size.width * win_size.height);

    /* load videos into plm */
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        const char *file_name = nob_temp_sprintf("res/%s.mpg", video_names[i]);
        plm_t *plm = plm_create_with_filename(file_name);
        if (!plm) {
            nob_log(NOB_ERROR, "could not open file %s", file_name);
            return 1;
        } else {
            video_textures.plms[i] = plm;
            plm_set_audio_enabled(plm, FALSE);
            int width  = plm_get_width(plm);
            int height = plm_get_height(plm);
            video_textures.aspects[i] = (float)width / height;
        }
    }

    /* decode one frame initialize from each video */
    plm_frame_t *frame = NULL;
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        frame = plm_decode_video(video_textures.plms[i]);
        size_t y_size  = frame->y.width  * frame->y.height;
        size_t cb_size = frame->cb.width * frame->cb.height;
        size_t cr_size = frame->cr.width * frame->cr.height;
        video_textures.initial_frames[i] = *frame; // do a shallow copy first
        video_textures.initial_frames[i].y.data  = malloc(y_size);
        video_textures.initial_frames[i].cb.data = malloc(cb_size);
        video_textures.initial_frames[i].cr.data = malloc(cr_size);
        if (!init_video_texture(frame->y.data, frame->y.width, frame->y.height, i, VIDEO_PLANE_Y))     return 1;
        if (!init_video_texture(frame->cb.data, frame->cb.width, frame->cb.height, i, VIDEO_PLANE_CB)) return 1;
        if (!init_video_texture(frame->cr.data, frame->cr.width, frame->cr.height, i, VIDEO_PLANE_CR)) return 1;
    }

    /* general state tracking */
    copy_camera_infos(camera_defaults, &cameras[1], NOB_ARRAY_LEN(camera_defaults));
    int cam_view_idx = 0;
    int cam_move_idx = 0;
    Camera *camera = &cameras[cam_view_idx];
    log_controls();
    int cam_order[4] = {0};
    Shader_Mode shader_mode = SHADER_MODE_MODEL;
    float vid_update_time = 0.0f;
    bool playing = false;

    if (!vk_comp_buff_staged_upload(&pc_layers[lod].buff, pc_layers[lod].items)) return 1;
    if (!vk_comp_buff_staged_upload(&frame_buff.buff, frame_buff.data))          return 1;
    if (!vk_ubo_init(&ubo.buff))                                                 return 1;
    if (!vk_create_storage_img(&storage_tex))                                    return 1;

    /* setup descriptors */
    if (!setup_ds_layouts())                          return 1;
    if (!setup_ds_pool())                             return 1;
    if (!setup_ds_sets(frame_buff.buff, storage_tex)) return 1;

    /* start the decoder thread */
    video_queue_init();
    pthread_t prod_thread;
    pthread_create(&prod_thread, NULL, producer, NULL);

    /* allocate descriptor sets based on layouts */
    for (size_t i = 0; i < MAX_LOD; i++) {
        VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(&cs_render_ds_layout, 1, pool)};
        if (!vk_alloc_ds(alloc, &pc_layers[i].set)) return false;
    }
    if (!update_render_ds_sets(ubo.buff, frame_buff.buff, lod))  return 1;

    /* create pipelines */
    if (!create_pipelines()) return 1;

    /* record commands for compute buffer */
    if (!build_compute_cmds(lod)) return 1;

    /* game loop */
    while (!window_should_close()) {
        /* input */
        update_camera_free(&cameras[cam_move_idx]);

        /* handle keyboard input */
        if (is_key_pressed(KEY_UP) || is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
            if (++lod < MAX_LOD) {
                wait_idle();
                if (!pc_layers[lod].buff.handle) {
                    if (!load_points(pc_layers[lod].name, &pc_layers[lod])) return 1;

                    /* upload new buffer and update descriptor sets */
                    if (!vk_comp_buff_staged_upload(&pc_layers[lod].buff, pc_layers[lod].items)) return 1;
                    if (!update_render_ds_sets(ubo.buff, frame_buff.buff, lod))  return 1;
                }

                /* log point count */
                size_t point_count = 0;
                for (size_t layer = 0; layer <= lod; layer++) point_count += pc_layers[layer].count;
                nob_log(NOB_INFO, "lod %zu (%zu points)", lod, point_count);

                /* rebuild compute commands */
                if (!build_compute_cmds(lod)) return 1;
            } else {
                nob_log(NOB_INFO, "max lod %zu reached", --lod);
            }
        }
        if (is_key_pressed(KEY_DOWN) || is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
            if (lod != 0) {
                size_t point_count = 0;
                for (size_t layer = 0; layer <= lod - 1; layer++) point_count += pc_layers[layer].count;
                nob_log(NOB_INFO, "lod %zu (%zu points)", lod - 1, point_count);
                --lod;

                /* rebuild compute commands */
                wait_idle();
                if (!build_compute_cmds(lod)) return 1;
            } else {
                nob_log(NOB_INFO, "min lod %zu reached", lod);
            }
        }
        if (is_key_down(KEY_F)) log_fps();
        if (is_key_pressed(KEY_R) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            record.collecting = true;
        if ((is_key_pressed(KEY_M) && !is_key_down(KEY_LEFT_SHIFT)) ||
             is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
            shader_mode = (shader_mode + 1) % SHADER_MODE_COUNT;
            log_shader_mode(shader_mode);
        }
        if ((is_key_pressed(KEY_M) && is_key_down(KEY_LEFT_SHIFT)) ||
             is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1)) {
            shader_mode = (shader_mode - 1 + SHADER_MODE_COUNT) % SHADER_MODE_COUNT;
            log_shader_mode(shader_mode);
        }
        if (is_key_pressed(KEY_C)) {
            cam_move_idx = (cam_move_idx + 1) % NOB_ARRAY_LEN(cameras);
            nob_log(NOB_INFO, "piloting camera %d", cam_move_idx);
        }
        if (is_key_pressed(KEY_SPACE)) {
            nob_log(NOB_INFO, "resetting camera defaults");
            copy_camera_infos(&cameras[1], camera_defaults, NOB_ARRAY_LEN(camera_defaults));
        }
        if (is_key_pressed(KEY_P) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
            playing = !playing;
        if (is_key_pressed(KEY_L)) log_cameras(&cameras[1], NUM_CCTVS);

        if (playing) vid_update_time += get_frame_time();

        /* start collecting frame rates to average */
        if (record.collecting) {
            nob_da_append(&record, get_fps());
            /* calculate the average frame rate, print results, and reset */
            if (record.count >= record.max) {
                size_t sum = 0;
                for (size_t i = 0; i < record.count; i++) sum += record.items[i];
                float ave = (float) sum / record.count;
                nob_log(NOB_INFO, "Average (N=%zu) FPS %.2f", record.count, ave);
                record.count = 0;
                record.collecting = false;
            }
        }

        /* grab the next video frames off of the queue */
        if (vid_update_time > 1.0f / 30.0f && playing) {
            if (video_dequeue()) {
                vid_update_time = 0.0f;
            } else {
                nob_log(NOB_INFO, "video could not dequeue, this shouldn't happen");
                return 1;
            }
        }

        /* compute shader submission */
        begin_mode_3d(*camera);
            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);
            get_cam_order(cameras, NOB_ARRAY_LEN(cameras), cam_order, NOB_ARRAY_LEN(cam_order));
            float blend_ratio = calc_blend_ratio(cameras, cam_order);
            if (!vk_compute_fence_wait()) return 1;
            if (!update_pc_ubo(&cameras[1], shader_mode, cam_order, blend_ratio, &ubo)) return 1;
            if (!vk_submit_compute()) return 1;
        end_mode_3d();

        /* draw command for screen space triangle (sst) */
        start_timer();
        if (!vk_begin_drawing()) return 1;
            vk_raster_sampler_barrier(storage_tex.img.handle);
            vk_begin_render_pass(BLACK);
            vk_draw_sst(gfx_pl, gfx_pl_layout, ds_sets[DS_SST]);
        end_drawing();
    }

    /* stop the video producer thread to avoid segfaults */
    pthread_cancel(prod_thread);
    pthread_join(prod_thread, NULL);
    video_queue_destroy();

    wait_idle();
    free(frame_buff.data);
    free(tex_buff.data);
    for (size_t i = 0; i < MAX_LOD; i++) {
        vk_buff_destroy(pc_layers[i].buff);
        free(pc_layers[i].items);
    }
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        plm_destroy(video_textures.plms[i]);
        free(video_textures.initial_frames[i].y.data);
        free(video_textures.initial_frames[i].cb.data);
        free(video_textures.initial_frames[i].cr.data);
        for (size_t j = 0; j < VIDEO_PLANE_COUNT; j++) {
            vk_buff_destroy(video_textures.stg_buffs[j + i * VIDEO_PLANE_COUNT]);
            vk_unload_texture(&video_textures.planes[j + i * VIDEO_PLANE_COUNT]);
        }
    }
    vk_buff_destroy(frame_buff.buff);
    vk_buff_destroy(ubo.buff);
    vk_buff_destroy(tex_buff.buff);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(cs_render_ds_layout);
    vk_destroy_ds_layout(cs_resolve_ds_layout);
    vk_destroy_ds_layout(gfx_ds_layout);
    vk_destroy_pl_res(cs_render_pl, cs_render_pl_layout);
    vk_destroy_pl_res(cs_resolve_pl, cs_resolve_pl_layout);
    vk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    vk_unload_texture(&storage_tex);
    close_window();
    return 0;
}
