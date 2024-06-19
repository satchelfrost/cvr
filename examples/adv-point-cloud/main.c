#include "cvr.h"
#include "ext/nob.h"
#include "geometry.h"

/* point cloud sizes */
#define S  "5060224"
#define M  "50602232"
#define L  "101204464"
#define XL "506022320"

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

void log_cameras(Camera *four_cameras)
{
    for (size_t i = 0; i < 4; i++) {
        Vector3 pos = four_cameras[i].position;
        Vector3 up  = four_cameras[i].up;
        Vector3 tg  = four_cameras[i].target;
        float fov   = four_cameras[i].fovy;
        nob_log(NOB_INFO, "Camera %d", i);
        nob_log(NOB_INFO, "    position %.2f %.2f %.2f", pos.x, pos.y, pos.z);
        nob_log(NOB_INFO, "    up       %.2f %.2f %.2f", up.x, up.y, up.z);
        nob_log(NOB_INFO, "    target   %.2f %.2f %.2f", tg.x, tg.y, tg.z);
        nob_log(NOB_INFO, "    fovy     %.2f", fov);
    }
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

Camera cameras[] = {
    { // Camera for image view_4_ffmpeg.png
        .position   = {-38.50, 15.30, -14.38},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {-18.10, 8.71, -7.94},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // Camera for image view_3_ffmpeg.png
        .position   = {12.24, 15.17, 69.39},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {-6.66, 8.88, 64.07},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // Camera for image view_1_ffmpeg.png
        .position   = {25.20, 9.68, 38.50},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {20.30, 8.86, 37.39},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    { // Camera for image view_2_ffmpeg.png
        .position   = {-54.02, 12.14, 22.01},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {-44.42, 9.74, 23.87},
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
    const char *image_name = "res/view_3_ffmpeg.png";
    Image img = load_image(image_name);
    if (!img.data) {
        nob_log(NOB_ERROR, "failed to load png file %s", image_name);
        return 1;
    }

    /* initialize window and Vulkan */
    init_window(1600, 900, "point cloud");
    set_target_fps(60);

    /* upload resources to GPU */
    Texture pc_tex = load_pc_texture_from_image(img);
    free(img.data);
    if (!upload_point_cloud(hres.buff, &hres.id)) return 1;
    if (!upload_point_cloud(lres.buff, &lres.id)) return 1;
    nob_da_free(hres.verts);
    nob_da_free(lres.verts);

    /* debug cube params */
    Vector3 cube_pos = {0.0f, 0.0f, 0.0f};
    float cube_speed = 7.0f;
    float cube_rot = 0.0f;

    bool use_hres = false;
    int cam_view_idx = 0;
    int cam_move_idx = 0;
    Camera *camera = &cameras[cam_view_idx];
    log_controls();
    nob_log(NOB_INFO, "piloting camera %d", cam_move_idx);
    nob_log(NOB_INFO, "viewing camera %d",  cam_view_idx);
    while (!window_should_close()) {
        log_fps();

        /* input */
        if (is_key_pressed(KEY_C)) {
            cam_move_idx = (cam_move_idx + 1) % 4;
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
        if (is_key_pressed(KEY_P)) log_cameras(cameras);
        update_camera_free(&cameras[cam_move_idx]);

        float dt = get_frame_time();
        if (is_key_down(KEY_RIGHT) && is_key_down(KEY_LEFT_SHIFT))
            cube_rot += dt;
        else if (is_key_down(KEY_RIGHT))
            cube_pos.x += cube_speed * dt;
        if (is_key_down(KEY_LEFT)  && is_key_down(KEY_LEFT_SHIFT))
            cube_rot -= dt;
        else if (is_key_down(KEY_LEFT))
            cube_pos.x -= cube_speed * dt;
        if (is_key_down(KEY_DOWN)) cube_pos.z += cube_speed * dt;
        if (is_key_down(KEY_UP))   cube_pos.z -= cube_speed * dt;
        if (is_key_down(KEY_E))    camera_move_up(&cameras[cam_move_idx],  dt);
        if (is_key_down(KEY_Q))    camera_move_up(&cameras[cam_move_idx], -dt);

        /* draw */
        begin_drawing(BLUE);
            begin_mode_3d(*camera);
            push_matrix();
                translate(cube_pos.x, cube_pos.y, cube_pos.z);;
                rotate_y(cube_rot);
                if (!draw_shape(SHAPE_CUBE)) return 1;
            pop_matrix();

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

            size_t id = (use_hres) ? hres.id : lres.id;
            if (!draw_point_cloud_adv(id)) return 1;
            update_cameras_ubo(cameras, cam_view_idx);

        end_mode_3d();
        end_drawing();
    }

    unload_pc_texture(pc_tex);
    destroy_point_cloud(hres.id);
    destroy_point_cloud(lres.id);
    close_window();
    return 0;
}
