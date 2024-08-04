#include "cvr.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>

#define NUM_POINTS 1000000

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

void log_fps()
{
    static int fps = -1;
    int curr_fps = get_fps();
    if (curr_fps != fps) {
        nob_log(NOB_INFO, "FPS %d", curr_fps);
        fps = curr_fps;
    }
}

float rand_float()
{
    return (float)rand() / RAND_MAX;
}

Point_Cloud gen_points()
{
    Vertices verts = {0};
    for (size_t i = 0; i < NUM_POINTS; i++) {
        float theta = PI * rand_float();
        float phi   = 2 * PI * rand_float();
        float r     = 10.0f * rand_float();
        Color color = color_from_HSV(r * 360.0f, 1.0f, 1.0f);
        Point_Vert vert = {
            .x = r * sin(theta) * cos(phi),
            .y = r * sin(theta) * sin(phi),
            .z = r * cos(theta),
            .r = color.r,
            .g = color.g,
            .b = color.b,
            .a = 255,
        };

        nob_da_append(&verts, vert);
    }

    Point_Cloud point_cloud = {
        .buff.items = verts.items,
        .buff.count = verts.count,
        .buff.size  = verts.count * sizeof(*verts.items),
        .verts = verts,
    };

    return point_cloud;
}

int main()
{
    Point_Cloud point_cloud = gen_points();

    /* initialize window and Vulkan */
    init_window(1600, 900, "point cloud");
    set_target_fps(60);
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    if (!upload_point_cloud(point_cloud.buff, &point_cloud.id)) return 1;
    nob_da_free(point_cloud.verts);

    while (!window_should_close()) {
        log_fps();
        update_camera_free(&camera);

        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(get_time() * 0.5);
            if (!draw_points(point_cloud.id, EXAMPLE_POINT_CLOUD)) return 1;
        end_mode_3d();
        end_drawing();
    }

    destroy_point_cloud(point_cloud.id);
    close_window();
    return 0;
}
