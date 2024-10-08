#include "cvr.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
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
    float x, y, z;
    unsigned char r, g, b, a;
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
    float16 camera_mvps[4];
    int idx;
    int shader_mode;
    int cam_0;
    int cam_1;
    int cam_2;
    int cam_3;
} Point_Cloud_Uniform;

Point_Cloud_Uniform uniform = {0};

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

        Point_Vert vert = {
            .x = x, .y = y, .z = z,
            .r = r, .g = g, .b = b, .a = 255,
        };
        nob_da_append(verts, vert);
    }

defer:
    nob_sb_free(sb);
    return result;
}

bool load_points(const char *name, Point_Cloud *point_cloud)
{
    bool result = true;

    Vertices verts = {0};
    if (!read_vtx(name, &verts)) {
        nob_log(NOB_ERROR, "failed to load point cloud");
        nob_log(NOB_ERROR, "this example requires private data");
        nob_return_defer(false);
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

void copy_camera_infos(Camera *dst, const Camera *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
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
    nob_log(NOB_INFO, "    [E] - Up");
    nob_log(NOB_INFO, "    [Q] - Down");
    nob_log(NOB_INFO, "    [Shift] - Fast movement");
    nob_log(NOB_INFO, "    Right Click + Mouse Movement = Rotation");
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "| Hot keys |");
    nob_log(NOB_INFO, "------------");
    nob_log(NOB_INFO, "    [M] - Shader mode (base model, camera overlap, single texture, or multi-texture)");
    nob_log(NOB_INFO, "    [C] - Change piloted camera");
    nob_log(NOB_INFO, "    [R] - Resolution toggle");
    nob_log(NOB_INFO, "    [V] - View change (also pilots current view)");
    nob_log(NOB_INFO, "    [P] - Print camera info");
    nob_log(NOB_INFO, "    [Space] - Reset cameras to default position");
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

bool update_pc_uniform(Camera *four_cameras, int shader_mode, int *cam_order, Point_Cloud_Uniform *uniform)
{
    bool result = true;

    Matrix model = {0};
    if (!get_matrix_tos(&model)) nob_return_defer(false);

    Matrix mvps[4] = {0};
    for (size_t i = 0; i < 4; i++) {
        Matrix view = MatrixLookAt(
            four_cameras[i].position,
            four_cameras[i].target,
            four_cameras[i].up
        );
        Matrix proj = get_proj(four_cameras[i]);
        Matrix view_proj = MatrixMultiply(view, proj);
        mvps[i] = MatrixMultiply(model, view_proj);
    }

    for (size_t i = 0; i < 4; i++)
        uniform->camera_mvps[i] = MatrixToFloatV(mvps[i]);
    uniform->cam_0 = cam_order[0];
    uniform->cam_1 = cam_order[1];
    uniform->cam_2 = cam_order[2];
    uniform->cam_3 = cam_order[3];
    uniform->shader_mode = shader_mode;
    uniform->idx = cam_order[3]; // farthest away camera

defer:
    return result;
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
    if (argc > 4) {
        init_window(atoi(argv[3]), atoi(argv[4]), "point cloud");
        set_window_pos(atoi(argv[1]), atoi(argv[2]));
    } else {
        init_window(1600, 900, "point cloud");
    }

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

    /* initialize and map the uniform data */
    Buffer buff = {
        .size  = sizeof(uniform),
        .count = 1,
        .items = &uniform,
    };
    if (!ubo_init(buff, EXAMPLE_ADV_POINT_CLOUD)) return 1;

    /* settings & logging*/
    copy_camera_infos(camera_defaults, &cameras[1], NOB_ARRAY_LEN(camera_defaults));
    bool use_hres = false;
    bool print_fps = false;
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
        if (print_fps) log_fps();

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
        if (is_key_pressed(KEY_F)) print_fps = !print_fps;
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
            if (!draw_points(vtx_id, EXAMPLE_ADV_POINT_CLOUD)) return 1;

            /* update uniform buffer */
            get_cam_order(cameras, NOB_ARRAY_LEN(cameras), cam_order, NOB_ARRAY_LEN(cam_order));
            if (!update_pc_uniform(&cameras[1], shader_mode, cam_order, &uniform)) return 1;

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
