#include "cvr.h"

typedef struct {
    VkRenderpass rp;

} Multiview;

int main()
{
    Camera camera = {
        .position = {2.0f, 2.0f, 2.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(500, 500, "stero render");

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
