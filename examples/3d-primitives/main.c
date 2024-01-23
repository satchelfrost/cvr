#include "cvr.h"
#include "ext/raylib-5.0/raymath.h"

#define FOVY_PERSPECTIVE    45.0f
#define WIDTH_ORTHOGRAPHIC  10.0f

#include "ext/nob.h"

typedef struct {
    Matrix *items;
    size_t capacity;
    size_t count;
} Matrices;

Matrices matrices = {0};

int main()
{
    Camera camera = {
        .position   = {0.0f, 0.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = FOVY_PERSPECTIVE,
        .projection = PERSPECTIVE,
    };

    Shape_Type shape = SHAPE_CUBE;

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

        if (is_key_pressed(KEY_C)) shape = (shape + 1) % SHAPE_COUNT;

        double time_spent = get_time();
        Matrix model = MatrixRotateY(-time_spent * 0.5f);
        nob_da_append(&matrices, model);
        model = MatrixMultiply(MatrixTranslate(2.0, 0, 0), model);
        nob_da_append(&matrices, model);
        model = MatrixMultiply(MatrixScale(0.5, 0.5, 0.5), model);
        model = MatrixMultiply(MatrixRotateX(time_spent * 0.5), model);
        nob_da_append(&matrices, model);

        // Matrix tet = MatrixIdentity();
        begin_mode_3d(camera);
            draw_shape(shape, matrices.items, matrices.count);
            // draw_shape(SHAPE_TETRAHEDRON, &tet, 1);
        end_mode_3d();

        matrices.count = 0;
    }
    close_window();
    return 0;
}
