#include "cvr.h"

#define FOVY_PERSPECTIVE    45.0f
#define WIDTH_ORTHOGRAPHIC  10.0f

int main()
{
    Camera3D camera = {
        .position   = {0.0f, 10.0f, 10.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = FOVY_PERSPECTIVE,
        .projection = CAMERA_PERSPECTIVE,
    };

    // camera.fovy = WIDTH_ORTHOGRAPHIC;
    // camera.projection = CAMERA_ORTHOGRAPHIC;
    init_window(800, 450, "Spinning Shapes");
    clear_background(BLUE);
    while (!window_should_close()) {
        begin_mode_3d(camera);
        draw();
    }
    close_window();
    return 0;
}
