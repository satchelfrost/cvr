#include "cvr.h"
#include "ext/nob.h"
#include "vertex.h"

#define read_attr(attr, sv)               \
    memcpy(&attr, sv.data, sizeof(attr)); \
    sv.data  += sizeof(attr);             \
    sv.count -= sizeof(attr);             \
    
typedef struct {
    Vertex *items;
    size_t capacity;
    size_t count;
} Vertices;

bool read_vtx(const char *file, Vertices *verts);

int main()
{
    Vertices verts = {0};
    if (!read_vtx("examples/point_cloud/arena.vtx", &verts))
        return 1;

    Camera camera = {
        .position   = {100.0f, 100.0f, 100.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(800, 600, "Point Cloud");
    enable_point_topology();
    size_t id = upload_mesh(verts.items, verts.count);

    while (!window_should_close()) {
        begin_drawing(BLUE);
            begin_mode_3d(camera);
                draw_mesh(id);
            end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}

bool read_vtx(const char *file, Vertices *verts)
{
    bool result = true;

    nob_log(NOB_INFO, "reading vtx file");
    Nob_String_Builder sb = {0};
    bool res = nob_read_entire_file(file, &sb);
    if (!res) {
        nob_log(NOB_ERROR, "failed to read file");
        nob_return_defer(false);
    }
    Nob_String_View sv = nob_sv_from_parts(sb.items, sb.count);
    size_t vtx_count = 0;
    memcpy(&vtx_count, sv.data, sizeof(vtx_count));
    nob_log(NOB_INFO, "Num vertices %zu", vtx_count);

    /* skip vertex count field */
    sv.data += 4;

    for (size_t i = 0; i < vtx_count; i++) {
        Vertex vtx = {0};
        read_attr(vtx.pos.x, sv);
        read_attr(vtx.pos.z, sv);
        read_attr(vtx.pos.y, sv);
        uint8_t r, g, b;
        read_attr(r, sv);
        read_attr(g, sv);
        read_attr(b, sv);
        vtx.color.x = r / 255.0f;
        vtx.color.y = g / 255.0f;
        vtx.color.z = b / 255.0f;
        nob_da_append(verts, vtx);
    }
    nob_log(NOB_INFO, "Read %zu vertices", verts->count);

defer:
    return result;
}
