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

Camera cameras[] = {
    {
        .position   = {0.0f, 1.0f, 5.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    {
        .position   = {0.0f, 1.0f, 10.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    {
        .position   = {0.0f, 1.0f, 20.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    }
};

int main()
{
    /* load resources into main memory */
    Point_Cloud hres = {0};
    if (!load_points("res/arena_"M"_f32.vtx", &hres)) return 1;
    Point_Cloud lres = {0};
    if (!load_points("res/arena_"S"_f32.vtx", &lres)) return 1;
    Image img = load_image("res/out.png");
    if (!img.data) {
        nob_log(NOB_ERROR, "failed to load png file");
        return 1;
    }

    /* initialize window and Vulkan */
    init_window(1600, 900, "point cloud");
    set_target_fps(60);

    /* upload resources to GPU */
    Texture tex = load_texture_from_image(img);
    free(img.data);
    if (!upload_point_cloud(hres.buff, &hres.id)) return 1;
    if (!upload_point_cloud(lres.buff, &lres.id)) return 1;
    nob_da_free(hres.verts);
    nob_da_free(lres.verts);

    bool use_hres = true;
    int cam_idx = 0;
    Camera *camera = &cameras[cam_idx];
    while (!window_should_close()) {
        log_fps();

        /* input */
        if (is_key_pressed(KEY_SPACE)) {
            cam_idx = (cam_idx + 1) % NOB_ARRAY_LEN(cameras);
            camera = &cameras[cam_idx];
        }
        if (is_key_pressed(KEY_P)) use_hres = !use_hres;
        update_camera_free(camera);

        /* draw */
        begin_drawing(BLUE);
        begin_mode_3d(*camera);
            if (!draw_shape(SHAPE_CUBE)) return 1;
            /* draw the other cameras */
            for (size_t i = 0; i < NOB_ARRAY_LEN(cameras); i++) {
                if (camera == &cameras[i]) continue;
                push_matrix();
                    look_at(cameras[i]);
                    if (!draw_shape_wireframe(SHAPE_CAM)) return 1;

                    rotate_z(PI);
                    translate(0.0f, 0.0f, 0.5f);
                    scale(1.0f * 1.333f * 0.75, 1.0f * 0.75, 1.0f * 0.75);
                    if (!draw_texture(tex, SHAPE_QUAD)) return 1;
                pop_matrix();
            }

            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);
            size_t id = (use_hres) ? hres.id : lres.id;
            if (!draw_point_cloud(id)) return 1;
        end_mode_3d();
        end_drawing();
    }

    unload_texture(tex);
    destroy_point_cloud(hres.id);
    destroy_point_cloud(lres.id);
    close_window();
    return 0;
}
