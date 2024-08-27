#include "cvr.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>

#define MAX_POINTS  100000000 // 100 million
#define MIN_POINTS  100000    // 100 thousand
#define MAX_FPS_REC 1000

typedef struct {
    float x, y, z;
    unsigned char r, g, b, a;
} Point_Vert;

typedef struct {
    Point_Vert *items;
    size_t count;
    size_t capacity;
    Buffer buff;
    size_t id;
    bool pending_change;
    const size_t max;
    const size_t min;
} Point_Cloud;

typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
    const size_t max;
    bool collecting;
} FPS_Record;

void gen_points(size_t num_points, Point_Cloud *pc)
{
    /* reset the point count to zero, but leave capacity allocated */
    pc->count = 0;

    for (size_t i = 0; i < num_points; i++) {
        float theta = PI * rand() / RAND_MAX;
        float phi   = 2.0f * PI * rand() / RAND_MAX;
        float r     = 10.0f * rand() / RAND_MAX;
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

        nob_da_append(pc, vert);
    }

    pc->buff.items = pc->items;
    pc->buff.count = pc->count;
    pc->buff.size  = pc->count * sizeof(*pc->items);
}

int main()
{
    /* generate a point cloud */
    size_t num_points = MIN_POINTS;
    Point_Cloud pc = {.min = MIN_POINTS, .max = MAX_POINTS};
    gen_points(num_points, &pc);
    FPS_Record record = {.max = MAX_FPS_REC};

    /* initialize window and Vulkan */
    init_window(1600, 900, "point cloud");
    Camera camera = {
        .position   = {10.0f, 10.0f, 10.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    if (!upload_point_cloud(pc.buff, &pc.id)) return 1;

    while (!window_should_close()) {
        /* input */
        update_camera_free(&camera);
        if (is_key_pressed(KEY_UP)) {
            num_points = (num_points * 10 > pc.max) ? pc.max : num_points * 10;
            pc.pending_change = true;
        }
        if (is_key_pressed(KEY_DOWN)) {
            num_points = (num_points / 10 < pc.min) ? pc.min : num_points / 10;
            pc.pending_change = true;
        }
        if (is_key_pressed(KEY_R)) record.collecting = true;

        /* upload point cloud if we've changed points */
        if (pc.pending_change) {
            destroy_point_cloud(pc.id);
            gen_points(num_points, &pc);
            if (!upload_point_cloud(pc.buff, &pc.id)) return 1;
            pc.pending_change = false;
        }

        /* collect the frame rate */
        if (record.collecting) {
            nob_da_append(&record, get_fps());
            if (record.count >= record.max) {
                /* print results and reset */
                size_t sum = 0;
                for (size_t i = 0; i < record.count; i++) sum += record.items[i];
                float ave = (float) sum / record.count;
                nob_log(NOB_INFO, "Average (N=%zu) FPS %.2f, %zu points", record.count, ave, pc.count);
                record.count = 0;
                record.collecting = false;
            }
        }

        /* drawing */
        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(get_time() * 0.5);
            if (!draw_points(pc.id, EXAMPLE_POINT_CLOUD)) return 1;
        end_mode_3d();
        end_drawing();
    }

    destroy_point_cloud(pc.id);
    nob_da_free(pc);
    close_window();
    return 0;
}
