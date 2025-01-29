#include "cvr.h"

int main()
{
    init_window(800, 600, "mixed raster");

    Camera camera = {
        .position   = {5.0f, 5.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };
    
    while (!window_should_close()) {
        begin_drawing(BLUE);
            begin_mode_3d(camera);
                draw_shape(SHAPE_CUBE);
            end_mode_3d();
        end_drawing();
    }

    close_window();

    return 0;
}
