#include "cvr.h"
#include "vk_ctx.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>
#include <float.h>

#define FRAME_BUFF_SZ 1600
#define MAX_FPS_REC 100
#define SUBGROUP_SZ 1024    // Subgroup size for render.comp
#define NUM_BATCHES 4       // Number of batches to dispatch
#define MAX_LOD 7
#define NUM_CCTVS 4

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
    int shader_mode;
    int cam_order_0;
    int cam_order_1;
    int cam_order_2;
    int cam_order_3;
    float closest_two_cams_ratio;
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
VkDescriptorSet cs_resolve_ds;
VkDescriptorSet gfx_sst; // screen space triangle
Vk_Texture textures[NUM_CCTVS];
float ratio;

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

int get_closest_camera(Camera *cameras, size_t count)
{
    Vector3 main_cam_pos = cameras[0].position;
    float shortest = FLT_MAX;
    int shortest_idx = -1;
    for (size_t i = 1; i < count; i++) {
        Vector3 cctv_pos = cameras[i].position;
        float dist_sqr = Vector3DistanceSqr(main_cam_pos, cctv_pos);
        if (dist_sqr < shortest) {
            shortest_idx = i - 1;
            shortest = dist_sqr;
        }
    }
    if (shortest_idx < 0) nob_log(NOB_ERROR, "Unknown camera index");

    return shortest_idx;
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

    if (sqr_distances[1].dist_sqr != 0.0f)
        ratio = sqr_distances[0].dist_sqr / sqr_distances[1].dist_sqr;
}

void copy_camera_infos(Camera *dst, const Camera *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
}

typedef enum {
    SHADER_MODE_BASE_MODEL,
    SHADER_MODE_PROGRESSIVE_COLOR,
    SHADER_MODE_SINGLE_TEX,
    SHADER_MODE_MULTI_TEX,
    SHADER_MODE_COUNT,
} Shader_Mode;

void log_shader_mode(Shader_Mode mode)
{
    switch (mode) {
    case SHADER_MODE_BASE_MODEL:
        nob_log(NOB_INFO, "Shader mode: base model");
        break;
    case SHADER_MODE_PROGRESSIVE_COLOR:
        nob_log(NOB_INFO, "Shader mode: progressive color");
        break;
    case SHADER_MODE_SINGLE_TEX:
        nob_log(NOB_INFO, "Shader mode: single texture");
        break;
    case SHADER_MODE_MULTI_TEX:
        nob_log(NOB_INFO, "Shader mode: multi-texture");
        break;
    default:
        nob_log(NOB_ERROR, "Shader mode: unrecognized %d", mode);
        break;
    }
}

bool load_texture(Image img, size_t img_idx)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer stg_buff;
    stg_buff.size  = img.width * img.height * format_to_size(img.format);
    stg_buff.count = img.width * img.height;
    if (!vk_stg_buff_init(&stg_buff, img.data, false)) return false;

    /* create the image */
    Vk_Image vk_img = {
        .extent  = {img.width, img.height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    };
    result = vk_img_init(
        &vk_img,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (!result) nob_return_defer(false);
    result = transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    if (!result) nob_return_defer(false);
    if (!vk_img_copy(vk_img.handle, stg_buff.handle, vk_img.extent)) nob_return_defer(false);
    result = transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    if (!result) nob_return_defer(false);

    /* create image view */
    VkImageView img_view;
    if (!vk_img_view_init(vk_img, &img_view)) nob_return_defer(false);

    /* create sampler */
    VkSampler sampler;
    if (!vk_sampler_init(&sampler)) return false;

    Vk_Texture texture = {
        .view = img_view,
        .sampler = sampler,
        .img = vk_img,
    };
    textures[img_idx] = texture;

defer:
    vk_buff_destroy(stg_buff);
    return result;

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

Frame_Buffer alloc_frame_buff()
{
    Frame_Buffer frame = {0};
    frame.buff.count = FRAME_BUFF_SZ * FRAME_BUFF_SZ;
    frame.buff.size  = sizeof(uint64_t) * frame.buff.count;
    frame.data = malloc(frame.buff.size);
    return frame;
}

void alloc_tex_buff(Image img)
{
    tex_buff.buff.count = img.width * img.height;
    tex_buff.buff.size = sizeof(uint32_t) * tex_buff.buff.count;
    tex_buff.data = malloc(tex_buff.buff.size);
    // uint32_t red = 0xff0000ff;
    for (size_t i = 0; i < tex_buff.buff.count; i++) {
        memcpy(tex_buff.data + i * sizeof(uint32_t), img.data + i * sizeof(uint32_t), sizeof(uint32_t));
        // memcpy(tex_buff.data + i * sizeof(uint32_t), &red, sizeof(uint32_t));
    }
}

bool setup_ds_layouts()
{
    /* render.comp shader layout */
    VkDescriptorSetLayoutBinding cs_render_bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(3, COMBINED_IMAGE_SAMPLER, COMPUTE_BIT)},
        {DS_BINDING(4, COMBINED_IMAGE_SAMPLER, COMPUTE_BIT)},
        {DS_BINDING(5, COMBINED_IMAGE_SAMPLER, COMPUTE_BIT)},
        {DS_BINDING(6, COMBINED_IMAGE_SAMPLER, COMPUTE_BIT)},
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
    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 1)},                       // 1 in render.comp
        {DS_POOL(STORAGE_BUFFER, 3 * MAX_LOD)},             // (2 in render.comp + 1 in resolve.comp) * MAX_LOD
        {DS_POOL(COMBINED_IMAGE_SAMPLER, 1 + 4 * MAX_LOD)}, // 1 in sst.frag + 4 in render.comp * MAX_LOD
        {DS_POOL(STORAGE_IMAGE, 1)},                        // 1 in resolve.comp
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
    VkDescriptorImageInfo img_infos[NUM_CCTVS] = {0};
    for (size_t i = 0; i < NUM_CCTVS; i++) {
        img_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_infos[i].imageView   = textures[i].view;
        img_infos[i].sampler     = textures[i].sampler;
    }
    VkWriteDescriptorSet writes[] = {
        /* render.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, pc_layers[lod].set, &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, pc_layers[lod].set, &pc_info)},
        {DS_WRITE_BUFF(2, STORAGE_BUFFER, pc_layers[lod].set, &frame_buff_info)},
        {DS_WRITE_IMG (3, COMBINED_IMAGE_SAMPLER, pc_layers[lod].set, &img_infos[0])},
        {DS_WRITE_IMG (4, COMBINED_IMAGE_SAMPLER, pc_layers[lod].set, &img_infos[1])},
        {DS_WRITE_IMG (5, COMBINED_IMAGE_SAMPLER, pc_layers[lod].set, &img_infos[2])},
        {DS_WRITE_IMG (6, COMBINED_IMAGE_SAMPLER, pc_layers[lod].set, &img_infos[3])},
    };
    vk_update_ds(NOB_ARRAY_LEN(writes), writes);

    return true;
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
        group_x = group_y = FRAME_BUFF_SZ / 16;
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

bool update_pc_ubo(Camera *four_cameras, int shader_mode, int *cam_order, Point_Cloud_UBO *ubo)
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
        Matrix proj = get_proj(four_cameras[i]);
        Matrix view_proj = MatrixMultiply(view, proj);
        Matrix mvp = MatrixMultiply(model, view_proj);
        ubo->data.cctv_mvps[i] = MatrixToFloatV(mvp);
    }

    ubo->data.cam_order_0 = cam_order[0];
    ubo->data.cam_order_1 = cam_order[1];
    ubo->data.cam_order_2 = cam_order[2];
    ubo->data.cam_order_3 = cam_order[3];
    ubo->data.shader_mode = shader_mode;
    ubo->data.closest_two_cams_ratio = ratio;

    memcpy(ubo->buff.mapped, &ubo->data, ubo->buff.size);

    return true;
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
    nob_log(NOB_INFO, "        [M] - Shader mode (base model, camera overlap, single texture, or multi-texture)");
    nob_log(NOB_INFO, "        [C] - Change piloted camera");
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
    nob_log(NOB_INFO, "        [Right Trigger] - shader mode");
    nob_log(NOB_INFO, "        [Front Face Right Button] - Record FPS");
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
    { // cctv 1
        .position   = {25.20, 9.68, 38.50},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {20.30, 8.86, 37.39},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // cctv 2
        .position   = {-54.02, 12.14, 22.01},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {-44.42, 9.74, 23.87},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // cctv 3
        .position   = {12.24, 15.17, 69.39},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {-6.66, 8.88, 64.07},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // cctv 4
        .position   = {-38.50, 15.30, -14.38},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {-18.10, 8.71, -7.94},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
};

Camera camera_defaults[4] = {0};

int main(int argc, char **argv)
{
    Vk_Texture storage_tex = {.img.extent = {1600, 900}};
    Frame_Buffer frame = alloc_frame_buff();
    Point_Cloud_UBO ubo = {.buff = {.count = 1, .size = sizeof(UBO_Data)}};
    FPS_Record record = {.max = MAX_FPS_REC};
    size_t lod = 0;
    if (!load_points(pc_layers[lod].name, &pc_layers[lod])) return 1;

    /* load images into memory */
    Image imgs[NUM_CCTVS] = {0};
    for (size_t i = 0; i < NUM_CCTVS; i++) {
        const char *img_name = nob_temp_sprintf("res/view_%d_ffmpeg.png", i + 1);
        imgs[i] = load_image(img_name);
        if (!imgs[i].data) {
            nob_log(NOB_ERROR, "failed to load png file %s", img_name);
            return 1;
        }
    }

    /* initialize window and Vulkan */
    if (argc > 4) {
        init_window(atoi(argv[3]), atoi(argv[4]), "point cloud");
        set_window_pos(atoi(argv[1]), atoi(argv[2]));
    } else {
        init_window(1600, 900, "arena point cloud rasterization");
    }

    /* camera state tracking */
    copy_camera_infos(camera_defaults, &cameras[1], NOB_ARRAY_LEN(camera_defaults));
    int cam_view_idx = 0;
    int cam_move_idx = 0;
    Camera *camera = &cameras[cam_view_idx];
    log_controls();
    int cam_order[4] = {0};
    Shader_Mode shader_mode = SHADER_MODE_BASE_MODEL;

    /* upload resources to GPU */
    for (size_t i = 0; i < NUM_CCTVS; i++) {
       if (!load_texture(imgs[i], i)) return 1;
       free(imgs[i].data);
    }
    if (!vk_comp_buff_staged_upload(&pc_layers[lod].buff, pc_layers[lod].items)) return 1;
    if (!vk_comp_buff_staged_upload(&frame.buff, frame.data))                    return 1;
    if (!vk_ubo_init(&ubo.buff))                                                 return 1;
    if (!vk_create_storage_img(&storage_tex))                                    return 1;

    /* setup descriptors */
    if (!setup_ds_layouts())                     return 1;
    if (!setup_ds_pool())                        return 1;
    if (!setup_ds_sets(frame.buff, storage_tex)) return 1;

    /* allocate descriptor sets based on layouts */
    for (size_t i = 0; i < MAX_LOD; i++) {
        VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(&cs_render_ds_layout, 1, pool)};
        if (!vk_alloc_ds(alloc, &pc_layers[i].set)) return false;
    }
    if (!update_render_ds_sets(ubo.buff, frame.buff, lod))  return 1;

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
                    if (!update_render_ds_sets(ubo.buff, frame.buff, lod))  return 1;
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
        if (is_key_pressed(KEY_R) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
            record.collecting = true;
        if (is_key_pressed(KEY_M) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
            shader_mode = (shader_mode + 1) % SHADER_MODE_COUNT;
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

        /* compute shader submission */
        begin_mode_3d(*camera);
            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);
            get_cam_order(cameras, NOB_ARRAY_LEN(cameras), cam_order, NOB_ARRAY_LEN(cam_order));
            if (!vk_compute_fence_wait()) return 1;
            if (!update_pc_ubo(&cameras[1], shader_mode, cam_order, &ubo)) return 1;
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

    wait_idle();
    free(frame.data);
    free(tex_buff.data);
    for (size_t i = 0; i < MAX_LOD; i++) {
        vk_buff_destroy(pc_layers[i].buff);
        free(pc_layers[i].items);
    }
    for (size_t i = 0; i < NUM_CCTVS; i++) vk_unload_texture(&textures[i]);
    vk_buff_destroy(frame.buff);
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
