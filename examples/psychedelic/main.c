#include "cvr.h"
#include "ext/raylib-5.0/raymath.h"
#include <stddef.h>

Shape_Type shape = SHAPE_TETRAHEDRON;

int main()
{
    Camera camera = {
        .position   = {0.0f, 5.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(800, 600, "Psychedelic");
    enable_point_topology();

    while (!window_should_close()) {
        if (is_key_pressed(KEY_SPACE)) shape = (shape + 1) % SHAPE_COUNT;

        begin_mode_3d(camera);
            clear_background(BLUE);
            push_matrix();
            for (size_t k = 0; k < 8; k++) {
                    rotate_z(45.0f * DEG2RAD);
                    double time = get_time();
                    float step = sin(time) * 0.5f + 0.5f;
                    for (size_t i = 0; i < 13; i++) {
                        push_matrix();
                        for (int j = -5; j < 5; j++) {
                            translate(0.0f, 0.0f, 0.2f * step * (float) j);
                            draw_shape(shape);
                        }
                        pop_matrix();
                        rotate_y(0.5f);
                    }
            }
            pop_matrix();
        end_mode_3d();
    }

    close_window();
    return 0;
}
