#include "cvr.h"
#include "ext/nob.h"
#include "geometry.h"

#define read_attr(attr, sv)                   \
    do {                                      \
        memcpy(&attr, sv.data, sizeof(attr)); \
        sv.data  += sizeof(attr);             \
        sv.count -= sizeof(attr);             \
    } while(0)

typedef struct {
    Vertex *items;
    size_t count;
    size_t capacity;
} Vertices;

bool read_vtx(const char *file, Vertices *verts)
{
    bool result = true;

    nob_log(NOB_INFO, "reading vtx file %s", file);
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(file, &sb)) {
        nob_log(NOB_ERROR, "failed to read %s", file);
        nob_return_defer(false);
    }

    Nob_String_View sv = nob_sv_from_parts(sb.items, sb.count);
    size_t vtx_count = 0;
    read_attr(vtx_count, sv);
    nob_log(NOB_INFO, "count %zu", vtx_count);

    for (size_t i = 0; i < vtx_count; i++) {
        float x, y, z;
        uint8_t r, g, b;
        read_attr(x, sv);
        read_attr(y, sv);
        read_attr(z, sv);
        read_attr(r, sv);
        read_attr(g, sv);
        read_attr(b, sv);

        Vertex vert = {
            .pos = {x, y, z},
            .color = {r / 255.0f, g / 255.0f, b / 255.0f},
        };
        nob_da_append(verts, vert);
    }

defer:
    nob_sb_free(sb);
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
        .position   = {-5.0f, 2.0f, 10.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    },
    {
        .position   = {-5.0f, 4.0f, 20.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    }
};

int main()
{
    Vertices verts = {0};
    // if (!read_vtx("res/arena_5060224_f32.vtx", &verts)) {
    if (!read_vtx("res/arena_50602232_f32.vtx", &verts)) {
    // if (!read_vtx("res/arena_506022320_f32.vtx", &verts)) {
        if (!read_vtx("res/flowers.vtx", &verts)) return 1;
    }
    nob_log(NOB_INFO, "Number of vertices %zu", verts.count);

    /* set the current camera */
    int cam_idx = 0;
    Camera *camera = &cameras[cam_idx];

    init_window(1600, 900, "point cloud");

    size_t id;
    Buffer_Descriptor desc = {
        .items = verts.items,
        .count = verts.count,
        .size  = verts.count * sizeof(*verts.items),
    };
    if (!upload_point_cloud(desc, &id)) return 1;

    int fps = 0;
    while (!window_should_close()) {
        int curr_fps = get_fps();
        if (curr_fps != fps) {
            nob_log(NOB_INFO, "FPS %d", curr_fps);
            fps = curr_fps;
        }

        if (is_key_pressed(KEY_SPACE)) {
            cam_idx = (cam_idx + 1) % NOB_ARRAY_LEN(cameras);
            camera = &cameras[cam_idx];
        }

        update_camera_free(camera);

        begin_drawing(BLUE);
        begin_mode_3d(*camera);
            /* draw the other cameras as cubes */
            for (size_t i = 0; i < NOB_ARRAY_LEN(cameras); i++) {
                if (camera == &cameras[i]) continue;
                push_matrix();
                    translate(
                        cameras[i].position.x,
                        cameras[i].position.y,
                        cameras[i].position.z
                    );
                    if (!draw_shape(SHAPE_CUBE)) return 1;
                pop_matrix();
            }

            translate(0.0f, 0.0f, -100.0f);
            rotate_x(-PI / 2);
            if (!draw_point_cloud(id)) return 1;
        end_mode_3d();
        end_drawing();
    }

    destroy_point_cloud(id);
    close_window();

    nob_da_free(verts);
    return 0;
}
