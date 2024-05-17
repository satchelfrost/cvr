#include "cvr.h"

int main()
{
    Camera camera = {
        .position   = {0.0f, 1.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(800, 800, "camera movement");

    while(!window_should_close()) {
        update_camera_free(&camera);

        begin_drawing(GREEN);
        begin_mode_3d(camera);

        for (int i = -10; i < 5; i += 2) {
            for (int j = -10; j < 5; j += 2) {
                push_matrix();
                    translate(i, 0.0f, j);
                    draw_shape(SHAPE_TETRAHEDRON);
                pop_matrix();
            }
        }

        end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}
