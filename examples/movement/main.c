#include "cvr.h"
#include "ext/nob.h"

int main()
{
    Camera camera = {
        .position = {0.0f, 1.0f, 5.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(800, 800, "camera movement");

    int x, y;
    float norm_x, norm_y;
    Vector3 shape_pos = {0.0f, 0.0f, 0.0f};

    while(!window_should_close()) {
        int new_x = get_mouse_x();
        int new_y = get_mouse_y();
        if (x != new_x || y != new_y) {
            x = new_x;
            y = new_y;
            norm_x = (float) x / 800.0f * 2.0f - 1.0f;
            norm_y = (float)-y / 800.0f * 2.0f + 1.0f;
            // nob_log(NOB_INFO, "mouse pos (x, y) = (%d, %d)", x, y);
            nob_log(NOB_INFO, "NDC pos (x, y) = (%.2f, %.2f)", norm_x, norm_y);
            // camera.position.y = y;
        }

        if (is_key_down(KEY_S)) shape_pos.z += 0.01f;
        if (is_key_down(KEY_W)) shape_pos.z -= 0.01f;
        if (is_key_down(KEY_A)) shape_pos.x -= 0.01f;
        if (is_key_down(KEY_D)) shape_pos.x += 0.01f;


        begin_drawing(GREEN);
        begin_mode_3d(camera);

            // draw_shape(SHAPE_TETRAHEDRON);
            // translate(5.0f * norm_x, 0.0f, 5.0f * norm_y);
            translate(shape_pos.x, 0.0f, shape_pos.z);
            draw_shape(SHAPE_TETRAHEDRON);
            // draw_shape(SHAPE_QUAD);

        end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}
