#include "cvr.h"

Shape_Type shape = SHAPE_TETRAHEDRON;

int main()
{
    Camera camera = {
        .position   = {10.0f, 10.0f, 10.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    if (!init_window(800, 800, "Waves")) return 1;

    while (!window_should_close()) {
        if (is_key_pressed(KEY_SPACE)) shape = (shape + 1) % SHAPE_COUNT;

        begin_drawing(DARKGRAY);
            begin_mode_3d(camera);
                double time = get_time();
                rotate_y(time * 0.5f);
                for (int i = -5; i < 5; i++) {
                    for (int j = -5; j < 5; j++) {
                        push_matrix();
                        translate(i, 1.0f, j);
                        scale(1.0f, sin(2 * time + i / 2.0f) + 1.0f, 1.0f);
                        scale(1.0f, cos(2 * time + j / 2.0f) + 1.0f, 1.0f);
                        draw_shape_wireframe(shape);
                        pop_matrix();
                    }
                }
            end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}
