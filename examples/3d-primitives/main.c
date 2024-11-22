#include "cvr.h"
#include "ext/raylib-5.0/raymath.h"
#include "ext/nob.h"

// Shape_Type shape = SHAPE_CUBE;

int main()
{
    nob_log(NOB_INFO, "Hello Quest");
    // Camera camera = {
    //     .position   = {0.0f, 2.0f, 5.0f},
    //     .target     = {0.0f, 0.0f, 0.0f},
    //     .up         = {0.0f, 1.0f, 0.0f},
    //     .fovy       = 45.0f,
    //     .projection = PERSPECTIVE,
    // };
    //
    init_window(800, 600, "Spinning Shapes");
    //
    // while (!window_should_close()) {
    //     if (is_key_pressed(KEY_SPACE)) shape = (shape + 1) % SHAPE_COUNT;
    //
    //     begin_drawing(BLUE);
    //         begin_mode_3d(camera);
    //             double time = get_time();
    //             for (size_t i = 0; i < 10; i++) {
    //                 rotate_y(time * 0.4f);
    //                 translate(0.1f, 0.0f, -0.65f);
    //                 draw_shape_wireframe(shape);
    //             }
    //             translate(0.1f, 0.1f, -0.5f);
    //             push_matrix();
    //                 rotate_x(-90 * DEG2RAD);
    //                 draw_shape_wireframe(SHAPE_TETRAHEDRON);
    //             pop_matrix();
    //             scale(0.5f, 0.5f, 0.5f);
    //             draw_shape(SHAPE_TETRAHEDRON);
    //         end_mode_3d();
    //     end_drawing();
    // }
    //
    // close_window();
    return 0;
}
