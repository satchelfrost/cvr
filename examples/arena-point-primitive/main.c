#include "cvr.h"
#include "ext/nob.h"
// #include "ext/raylib-5.0/raymath.h"
#include <float.h>
#include "vk_ctx.h"

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

/* point cloud sizes */
#define S  "5060224"
#define M  "50602232"
#define L  "101204464"
#define XL "506022320"
#define TEXTURE_COUNT 4

#define read_attr(attr, sv)                   \
    do {                                      \
        memcpy(&attr, sv.data, sizeof(attr)); \
        sv.data  += sizeof(attr);             \
        sv.count -= sizeof(attr);             \
    } while(0)

typedef struct {
    float x, y, z;
    unsigned char r, g, b, a;
} Point_Vert;

typedef struct {
    Point_Vert *items;
    size_t count;
    size_t capacity;
    Vk_Buffer buff;
} Point_Cloud;

typedef struct {
    const char *file_name;
    Vk_Texture handle;
    float aspect;
} Texture;

typedef struct {
    float16 camera_mvps[TEXTURE_COUNT];
    int idx; // TODO: unused in shader
    int shader_mode;
    int cam_0;
    int cam_1;
    int cam_2;
    int cam_3;
} UBO_Data;

typedef struct {
    Vk_Buffer buff;
    UBO_Data data;
} Point_Cloud_UBO;

typedef enum {
    DS_SET_VERT,
    DS_SET_FRAG,
    DS_SET_COUNT,
} Ds_Set_Idx;

typedef enum {
    DS_LAYOUT_UNIFORM,
    DS_LAYOUT_SAMPLERS,
    DS_LAYOUT_COUNT,
} Ds_Layout_Idx;

Texture textures[TEXTURE_COUNT] = {
    {.file_name = "res/view_1_ffmpeg.png"},
    {.file_name = "res/view_2_ffmpeg.png"},
    {.file_name = "res/view_3_ffmpeg.png"},
    {.file_name = "res/view_4_ffmpeg.png"},
};
VkDescriptorSetLayout ds_layouts[DS_LAYOUT_COUNT];
VkDescriptorSet ds_sets[DS_SET_COUNT];
VkDescriptorPool pool;
VkPipelineLayout pl_layout;
VkPipeline gfx_pl;

bool setup_ds_layouts()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        /* point-cloud.vert */
        {DS_BINDING(0, UNIFORM_BUFFER, VERTEX_BIT)},
        /* point-cloud.frag */
        {DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)},
        {DS_BINDING(1, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)},
        {DS_BINDING(2, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)},
        {DS_BINDING(3, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)},
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = bindings,
    };
    if (!vk_create_ds_layout(layout_ci, &ds_layouts[DS_LAYOUT_UNIFORM])) return false;

    layout_ci.bindingCount = 4;
    layout_ci.pBindings = &bindings[1];
    if (!vk_create_ds_layout(layout_ci, &ds_layouts[DS_LAYOUT_SAMPLERS])) return false;

    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 1)},
        {DS_POOL(COMBINED_IMAGE_SAMPLER, 4)},
    };
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = NOB_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 2,
    };
    return vk_create_ds_pool(pool_ci, &pool);
}

bool setup_ds_sets(Vk_Buffer buff)
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(ds_layouts, DS_LAYOUT_COUNT, pool)};
    if (!vk_alloc_ds(alloc, ds_sets)) return false;

    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = buff.handle,
        .range  = buff.size,
    };
    VkDescriptorImageInfo img_infos[TEXTURE_COUNT] = {0};
    for (size_t i = 0; i < TEXTURE_COUNT; i++) {
        img_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_infos[i].imageView   = textures[i].handle.view;
        img_infos[i].sampler     = textures[i].handle.sampler;
    }
    VkWriteDescriptorSet writes[] = {
        /* point-cloud.vert */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, ds_sets[DS_SET_VERT], &ubo_info)},
        /* point-cloud.frag */
        {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, ds_sets[DS_SET_FRAG], &img_infos[0])},
        {DS_WRITE_IMG(1, COMBINED_IMAGE_SAMPLER, ds_sets[DS_SET_FRAG], &img_infos[1])},
        {DS_WRITE_IMG(2, COMBINED_IMAGE_SAMPLER, ds_sets[DS_SET_FRAG], &img_infos[2])},
        {DS_WRITE_IMG(3, COMBINED_IMAGE_SAMPLER, ds_sets[DS_SET_FRAG], &img_infos[3])},
    };
    vk_update_ds(NOB_ARRAY_LEN(writes), writes);

    return true;
}

bool upload_texture(Texture *texture)
{
    bool result = true;
    int channels, width, height;
    void *data = stbi_load(texture->file_name, &width, &height, &channels, STBI_rgb_alpha);

    if (!data) {
        nob_log(NOB_ERROR, "image %s could not be loaded", texture->file_name);
        nob_return_defer(false);
    } else {
        nob_log(NOB_INFO, "image %s was successfully loaded", texture->file_name);
        nob_log(NOB_INFO, "    (height, width) = (%d, %d)", height, width);
        nob_log(NOB_INFO, "    image size in memory = %d bytes", height * width * 4);
    }

    if (!vk_load_texture(data, width, height, VK_FORMAT_R8G8B8A8_SRGB, &texture->handle))
        nob_return_defer(false);

    texture->aspect = (float)width / height;

defer:
    free(data);
    return result;
}

bool read_vtx(const char *file, Point_Cloud *pc) // TODO: I don't think this needs to be a pointer
{
    bool result = true;

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

        Point_Vert vert = {
            .x = x, .y = y, .z = z,
            .r = r, .g = g, .b = b, .a = 255,
        };
        nob_da_append(pc, vert);
    }

defer:
    nob_sb_free(sb);
    return result;
}

bool load_points(const char *name, Point_Cloud *point_cloud)
{
    bool result = true;

    if (!read_vtx(name, point_cloud)) {
        nob_log(NOB_ERROR, "failed to load point cloud");
        nob_log(NOB_ERROR, "this example requires private data");
        nob_return_defer(false);
    }
    nob_log(NOB_INFO, "Number of vertices %zu", point_cloud->count);

    // point_cloud->buff.items = verts.items;
    point_cloud->buff.count = point_cloud->count;
    point_cloud->buff.size  = point_cloud->count * sizeof(*point_cloud->items);
    // point_cloud->verts = verts;

defer:
    return result;
}

void log_cameras(Camera *cameras, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        Vector3 pos = cameras[i].position;
        Vector3 up  = cameras[i].up;
        Vector3 tg  = cameras[i].target;
        float fov   = cameras[i].fovy;
        nob_log(NOB_INFO, "Camera %d", i);
        nob_log(NOB_INFO, "    position %.2f %.2f %.2f", pos.x, pos.y, pos.z);
        nob_log(NOB_INFO, "    up       %.2f %.2f %.2f", up.x, up.y, up.z);
        nob_log(NOB_INFO, "    target   %.2f %.2f %.2f", tg.x, tg.y, tg.z);
        nob_log(NOB_INFO, "    fovy     %.2f", fov);
    }
}

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
}

void copy_camera_infos(Camera *dst, const Camera *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
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
    nob_log(NOB_INFO, "        [R] - Resolution toggle");
    nob_log(NOB_INFO, "        [V] - View change (also pilots current view)");
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

bool update_pc_ubo(Camera *four_cameras, int shader_mode, int *cam_order, Point_Cloud_UBO *ubo)
{
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
        ubo->data.camera_mvps[i] = MatrixToFloatV(mvp);
    }

    ubo->data.cam_0 = cam_order[0];
    ubo->data.cam_1 = cam_order[1];
    ubo->data.cam_2 = cam_order[2];
    ubo->data.cam_3 = cam_order[3];
    ubo->data.shader_mode = shader_mode;
    ubo->data.idx = cam_order[3]; // TODO: unused in shader

    memcpy(ubo->buff.mapped, &ubo->data, ubo->buff.size);

    return true;
}

bool create_pipelines()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float16)};
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = DS_LAYOUT_COUNT,
        .pSetLayouts = ds_layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!vk_pl_layout_init(layout_ci, &pl_layout)) return 1;

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
        .pl_layout = pl_layout,
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
    /* load resources into main memory */
    Point_Cloud hres = {0};
    if (!load_points("res/arena_" M "_f32.vtx", &hres)) return 1;
    Point_Cloud lres = {0};
    if (!load_points("res/arena_" S "_f32.vtx", &lres)) return 1;

    /* initialize window and Vulkan */
    if (argc > 4) {
        init_window(atoi(argv[3]), atoi(argv[4]), "arena point primitive");
        set_window_pos(atoi(argv[1]), atoi(argv[2]));
    } else {
        init_window(1600, 900, "arena point primitive");
    }

    /* upload resources to GPU */
    for (size_t i = 0; i < TEXTURE_COUNT; i++) if (!upload_texture(&textures[i])) return 1;
    if (!vk_vtx_buff_staged_upload(&hres.buff, hres.items)) return 1;
    if (!vk_vtx_buff_staged_upload(&lres.buff, lres.items)) return 1;
    nob_da_free(hres);
    nob_da_free(lres);

    /* initialize shader resources */
    Point_Cloud_UBO ubo = {.buff = {.count = 1, .size = sizeof(UBO_Data)}};
    if (!vk_ubo_init(&ubo.buff))  return 1;
    if (!setup_ds_layouts())      return 1;
    if (!setup_ds_pool())         return 1;
    if (!setup_ds_sets(ubo.buff)) return 1;

    if (!create_pipelines()) return 1;

    /* settings & logging*/
    copy_camera_infos(camera_defaults, &cameras[1], NOB_ARRAY_LEN(camera_defaults));
    bool use_hres = false;
    int cam_view_idx = 0;
    int cam_move_idx = 0;
    Camera *camera = &cameras[cam_view_idx];
    log_controls();
    nob_log(NOB_INFO, "piloting camera %d", cam_move_idx);
    nob_log(NOB_INFO, "viewing camera %d",  cam_view_idx);
    Shader_Mode shader_mode = SHADER_MODE_BASE_MODEL;
    int cam_order[4] = {0};

    /* game loop */
    while (!window_should_close()) {
        /* input */
        if (is_key_down(KEY_F)) log_fps();
        if (is_key_pressed(KEY_C)) {
            cam_move_idx = (cam_move_idx + 1) % NOB_ARRAY_LEN(cameras);
            nob_log(NOB_INFO, "piloting camera %d", cam_move_idx);
        }
        if (is_key_pressed(KEY_V)) {
            cam_view_idx = (cam_view_idx + 1) % NOB_ARRAY_LEN(cameras);
            cam_move_idx = cam_view_idx;
            camera = &cameras[cam_view_idx];
            nob_log(NOB_INFO, "viewing camera %d", cam_view_idx);
            nob_log(NOB_INFO, "piloting camera %d", cam_move_idx);
        }
        if (is_key_pressed(KEY_R) || is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_FACE_UP))
            use_hres = !use_hres;
        if (is_key_pressed(KEY_P)) log_cameras(cameras, NOB_ARRAY_LEN(cameras));
        if (is_key_pressed(KEY_M) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
            shader_mode = (shader_mode + 1) % SHADER_MODE_COUNT;
            log_shader_mode(shader_mode);
        }
        if (is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1)) {
            shader_mode = (shader_mode - 1 + SHADER_MODE_COUNT) % SHADER_MODE_COUNT;
            log_shader_mode(shader_mode);
        }
        if (is_key_pressed(KEY_SPACE)) {
            nob_log(NOB_INFO, "resetting camera defaults");
            copy_camera_infos(&cameras[1], camera_defaults, NOB_ARRAY_LEN(camera_defaults));
        }
        update_camera_free(&cameras[cam_move_idx]);

        /* draw */
        begin_drawing(BLUE);
            begin_mode_3d(*camera);
            /* draw the other cameras */
            for (size_t i = 0; i < NOB_ARRAY_LEN(cameras); i++) {
                if (camera == &cameras[i]) continue;
                push_matrix();
                    look_at(cameras[i]);
                    if (!draw_shape_wireframe(SHAPE_CAM)) return 1;
                pop_matrix();
            }
            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);
            draw_points_ex2((use_hres) ? hres.buff : lres.buff, gfx_pl, pl_layout, ds_sets, DS_SET_COUNT);

            /* update uniform buffer */
            get_cam_order(cameras, NOB_ARRAY_LEN(cameras), cam_order, NOB_ARRAY_LEN(cam_order));
            if (!update_pc_ubo(&cameras[1], shader_mode, cam_order, &ubo)) return 1;
        end_mode_3d();
        end_drawing();
    }

    /* cleanup */
    wait_idle();
    for (size_t i = 0; i < TEXTURE_COUNT; i++) vk_unload_texture(&textures[i].handle);
    vk_buff_destroy(ubo.buff);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(ds_layouts[DS_LAYOUT_UNIFORM]);
    vk_destroy_ds_layout(ds_layouts[DS_LAYOUT_SAMPLERS]);
    vk_destroy_pl_res(gfx_pl, pl_layout);
    vk_buff_destroy(hres.buff);
    vk_buff_destroy(lres.buff);
    close_window();
    return 0;
}
