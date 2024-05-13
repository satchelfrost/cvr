#include "cvr.h"

int main()
{
    Camera camera = {
        .position = {0.0f, 1.0f, 2.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(500, 500, "depth");

    while(!window_should_close()) {
        double time = get_time();

        begin_drawing(BEIGE);
        begin_mode_3d(camera);
            rotate_y(time);
            if (!draw_shape(SHAPE_CUBE)) return 1;
        end_mode_3d();
        end_drawing();
    }

    return 0;
}
