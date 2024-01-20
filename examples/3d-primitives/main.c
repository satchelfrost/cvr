#include "cvr.h"

#define FOVY_PERSPECTIVE    45.0f
#define WIDTH_ORTHOGRAPHIC  10.0f

int main()
{
    Camera camera = {
        .position   = {0.0f, 0.0f, 10.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = FOVY_PERSPECTIVE,
        .projection = PERSPECTIVE,
    };

    init_window(800, 600, "Spinning Shapes");
    clear_background(BLUE);
    while (!window_should_close()) {
        if (is_key_pressed(KEY_SPACE)) {
            if (camera.projection == PERSPECTIVE) {
                camera.fovy = WIDTH_ORTHOGRAPHIC;
                camera.projection = ORTHOGRAPHIC;
                set_cube_color(GREEN);
            } else {
                camera.fovy = FOVY_PERSPECTIVE;
                camera.projection = PERSPECTIVE;
                set_cube_color(YELLOW);
            }
        }

        begin_mode_3d(camera);
            draw();
        end_mode_3d();
    }
    close_window();
    return 0;
}
