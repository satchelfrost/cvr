#include "cvr.h"

int main()
{
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(800, 600, "Compute Shader");

    while (!window_should_close()) {
        begin_drawing(RED);
        begin_mode_3d(camera);
        end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}
