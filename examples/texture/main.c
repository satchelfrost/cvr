#include "cvr.h"
#include "ext/nob.h"
#include <math.h>

bool load_texture(const char *name, Texture *texture)
{
    Image img = load_image(name);

    if (!img.data) {
        nob_log(NOB_ERROR, "image %s could not be loaded", name);
        return false;
    } else {
        nob_log(NOB_INFO, "image %s was successfully loaded", name);
        nob_log(NOB_INFO, "    (height, width) = (%d, %d)", img.height, img.width);
        nob_log(NOB_INFO, "    image size in memory = %d bytes", img.height * img.width * 4);
    }

    *texture = load_texture_from_image(img);
    nob_log(NOB_INFO, "    texture id %d", texture->id);
    unload_image(img);

    return true;
}

int main()
{
    Camera camera = {
        .position   = {0.0f, 1.0f, 4.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(500, 500, "Load texture");

    Texture matrix_tex = {0};
    if (!load_texture("res/matrix.png", &matrix_tex)) return 1;
    float matrix_aspect = (float)matrix_tex.width / matrix_tex.height;
    Texture statue_tex = {0};
    if (!load_texture("res/statue.jpg", &statue_tex)) return 1;
    float statue_aspect = (float)statue_tex.width / statue_tex.height;

    while(!window_should_close()) {
        begin_drawing(BLACK);
        begin_mode_3d(camera);
            double time = get_time();
            rotate_z(3.14259f);
            rotate_y(time);
            push_matrix();
                scale(matrix_aspect, 1.0f, 1.0f);
                if (!draw_texture(matrix_tex, SHAPE_QUAD)) return 1;
            pop_matrix();

            scale(statue_aspect, 1.0f, 1.0f);
            translate(0.0f, 0.0f, 1.0f);
            if (!draw_texture(statue_tex, SHAPE_QUAD)) return 1;
            rotate_x(time);
            draw_shape_wireframe(SHAPE_CUBE);

        end_mode_3d();
        end_drawing();
    }

    unload_texture(matrix_tex);
    unload_texture(statue_tex);
    close_window();
    return 0;
}
