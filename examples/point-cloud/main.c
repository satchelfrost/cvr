#include "cvr.h"
#include "ext/nob.h"
#include "geometry.h"

#define read_attr(attr, sv)                   \
    do {                                      \
        memcpy(&attr, sv.data, sizeof(attr)); \
        sv.data  += sizeof(attr);             \
        sv.count += sizeof(attr);             \
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
    memcpy(&vtx_count, sv.data, sizeof(vtx_count));

    /* skip vertex count field */
    sv.data += 4;

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


int main()
{
    Vertices verts = {0};
    if (!read_vtx("res/arena_5060224_f32.vtx", &verts)) return 1;
    nob_log(NOB_INFO, "Number of vertices %zu", verts.count);

    Camera camera = {
        .position   = {0.0f, 1.0f, 2.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(1600, 900, "point cloud");

    size_t id;
    Buffer_Descriptor desc = {
        .items = verts.items,
        .count = verts.count,
        .size  = verts.count * sizeof(*verts.items),
    };
    if (!upload_point_cloud(desc, &id)) return 1;

    while (!window_should_close()) {
        update_camera_free(&camera);

        begin_drawing(BLUE);
        begin_mode_3d(camera);
            if (!draw_shape(SHAPE_CUBE)) return 1;
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
