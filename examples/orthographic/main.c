#include "cvr.h"

#define FOV_PERSPECTIVE 45.0f
#define WIDTH_ORTHOGRAPHIC 10.0f

int main()
{
    Camera camera = {
        .position   = {0.0f, 10.0f, 10.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = FOV_PERSPECTIVE,
        .projection = PERSPECTIVE,
    };

    init_window(800, 450, "orthographic");

    while (!window_should_close()) {
        if (is_key_pressed(KEY_SPACE)) {
            if (camera.projection == PERSPECTIVE) {
                camera.fovy = WIDTH_ORTHOGRAPHIC;
                camera.projection = ORTHOGRAPHIC;
            } else {
                camera.fovy = FOV_PERSPECTIVE;
                camera.projection = PERSPECTIVE;
            }
        }

        begin_mode_3d(camera);
            clear_background(PURPLE);
            push_matrix();
                rotate_y(0.5);
                draw_shape(SHAPE_TETRAHEDRON);
            pop_matrix();
            push_matrix();
                translate(-4.0f, 0.0f, 2.0f);
                scale(2.0f, 5.0f, 2.0f);
                draw_shape(SHAPE_CUBE);
            pop_matrix();
            push_matrix();
                translate(4.0f, 0.0f, 2.0f);
                scale(2.0f, 3.0f, 2.0f);
                draw_shape(SHAPE_QUAD);
            pop_matrix();
        end_mode_3d();
    }

    close_window();
    return 0;
}
