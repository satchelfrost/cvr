#include "cvr.h"

int main()
{
    Camera camera = {
        .position   = {0.0f, 1.0f, 2.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(500, 500, "cube");

    while(!window_should_close()) {
        if (is_key_down(KEY_UP)) {
            camera.fovy += 10*get_frame_time();
            printf("%f\n", camera.fovy);
        } 
        if (is_key_down(KEY_DOWN)) {
            camera.fovy -= 10*get_frame_time();
            printf("%f\n", camera.fovy);
        }
        update_camera_free(&camera);
        begin_drawing(BEIGE);
        begin_mode_3d(camera);
            rotate_y(get_time());
            if (!draw_shape(SHAPE_CUBE)) return 1;
        end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}
