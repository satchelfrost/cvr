#include "cvr.h"
#include "ext/nob.h"
#include "geometry.h"
#include <float.h>

/* point cloud sizes */
#define S  "5060224"
#define M  "50602232"
#define L  "101204464"
#define XL "506022320"
#define NUM_IMGS 4

#define read_attr(attr, sv)                   \
    do {                                      \
        memcpy(&attr, sv.data, sizeof(attr)); \
        sv.data  += sizeof(attr);             \
        sv.count -= sizeof(attr);             \
    } while(0)

typedef struct {
    Small_Vertex *items;
    size_t count;
    size_t capacity;
} Vertices;

typedef struct {
    Vertices verts;
    Buffer buff;
    size_t id;
} Point_Cloud;

bool read_vtx(const char *file, Vertices *verts)
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

        Small_Vertex vert = {
            .pos = {x, y, z},
            .r = r, .g = g, .b = b,
        };
        nob_da_append(verts, vert);
    }

defer:
    nob_sb_free(sb);
    return result;
}

void log_fps()
{
    static int fps = -1;
    int curr_fps = get_fps();
    if (curr_fps != fps) {
        nob_log(NOB_INFO, "FPS %d", curr_fps);
        fps = curr_fps;
    }
}

bool load_points(const char *name, Point_Cloud *point_cloud)
{
    bool result = true;

    Vertices verts = {0};
    if (!read_vtx(name, &verts)) {
        nob_log(NOB_WARNING, "loading default instead");
        if (!read_vtx("res/flowers.vtx", &verts)) {
            nob_log(NOB_ERROR, "failed to load default point cloud");
            nob_return_defer(false);
        }
    }
    nob_log(NOB_INFO, "Number of vertices %zu", verts.count);

    point_cloud->buff.items = verts.items;
    point_cloud->buff.count = verts.count;
    point_cloud->buff.size  = verts.count * sizeof(*verts.items);
    point_cloud->verts = verts;

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
        float distSqr = Vector3DistanceSqr(main_cam_pos, cctv_pos);
        if (distSqr < shortest) {
            shortest_idx = i - 1;
            shortest = distSqr;
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
void get_cam_order(Camera *cameras, size_t count, int *cam_order, size_t cam_order_count)
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

void log_controls()
{
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "| Movement |");
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "    [W] - Forward");
    nob_log(NOB_INFO, "    [A] - Left");
    nob_log(NOB_INFO, "    [S] - Back");
    nob_log(NOB_INFO, "    [D] - Right");
    nob_log(NOB_INFO, "    Right Click + Mouse Movement = Rotation");
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "| Hot keys |");
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "    [C] - Change piloted camera");
    nob_log(NOB_INFO, "    [R] - Resolution toggle");
    nob_log(NOB_INFO, "    [V] - View change (also pilots current view)");
    nob_log(NOB_INFO, "    [P] - Print camera info");
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

int main()
{
    /* load resources into main memory */
    Point_Cloud hres = {0};
    if (!load_points("res/arena_"M"_f32.vtx", &hres)) return 1;
    Point_Cloud lres = {0};
    if (!load_points("res/arena_"S"_f32.vtx", &lres)) return 1;
    Image imgs[NUM_IMGS] = {0};
    for (size_t i = 0; i < NUM_IMGS; i++) {
        const char *img_name = nob_temp_sprintf("res/view_%d_ffmpeg.png", i + 1);
        imgs[i] = load_image(img_name);
        if (!imgs[i].data) {
            nob_log(NOB_ERROR, "failed to load png file %s", img_name);
            return 1;
        }
    }

    /* initialize window and Vulkan */
    init_window(1600, 900, "point cloud");
    set_target_fps(60);

    /* upload resources to GPU */
    Texture texs[NUM_IMGS] = {0};
    for (size_t i = 0; i < NUM_IMGS; i++) {
        texs[i] = load_pc_texture_from_image(imgs[i]);
        free(imgs[i].data);
    }
    if (!upload_point_cloud(hres.buff, &hres.id)) return 1;
    if (!upload_point_cloud(lres.buff, &lres.id)) return 1;
    nob_da_free(hres.verts);
    nob_da_free(lres.verts);

    /* settings & logging*/
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
        if (is_key_pressed(KEY_R)) use_hres = !use_hres;
        if (is_key_pressed(KEY_P)) log_cameras(cameras, NOB_ARRAY_LEN(cameras));
        if (is_key_pressed(KEY_M)) {
            shader_mode = (shader_mode + 1) % SHADER_MODE_COUNT;
            log_shader_mode(shader_mode);
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

                    rotate_z(PI);
                    translate(0.0f, 0.0f, 0.5f);
                    scale(1.0f * 1.333f * 0.75, 1.0f * 0.75, 1.0f * 0.75);
                pop_matrix();
            }
            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);

            size_t vtx_id = (use_hres) ? hres.id : lres.id;
            if (!draw_point_cloud_adv(vtx_id)) return 1;
            get_cam_order(cameras, NOB_ARRAY_LEN(cameras), cam_order, NOB_ARRAY_LEN(cam_order));
            update_cameras_ubo(&cameras[1], shader_mode, cam_order);

        end_mode_3d();
        end_drawing();
    }

    /* cleanup */
    for (size_t i = 0; i < NUM_IMGS; i++) unload_pc_texture(texs[i]);
    destroy_point_cloud(hres.id);
    destroy_point_cloud(lres.id);
    close_window();
    return 0;
}
