#include "cvr.h"
#include "ext/nob.h"
#include <math.h>

int main()
{
    Camera camera = {
        .position   = {0.0f, 0.0f, 2.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    const int width  = 500;
    const int height = 500;
    init_window(width, height, "Load texture");

    const char *img_name = "res/matrix.png";
    Image img = load_image(img_name);
    if (!img.data) {
        nob_log(NOB_ERROR, "image %s could not be loaded", img_name);
        return 1;
    } else {
        nob_log(NOB_INFO, "image %s was successfully loaded", img_name);
        nob_log(NOB_INFO, "    (height, width) = (%d, %d)", img.height, img.width);
        nob_log(NOB_INFO, "    image size in memory = %d bytes", img.height * img.width * 4);
    }
    float aspect_ratio = (float)img.width / img.height;

    Texture tex = load_texture_from_image(img);
    nob_log(NOB_INFO, "    texture id %d", tex.id);
    unload_image(img);

    Image img_2 = load_image("res/statue.jpg");
    Texture tex_2 = load_texture_from_image(img_2);
    unload_image(img_2);

    while(!window_should_close()) {
        begin_drawing(BLACK);
        begin_mode_3d(camera);

            double time = get_time();
            rotate_z(3.14259f);
            scale((sin(time) + 2.0f) * aspect_ratio / 2.0f, (sin(time) + 2.0f) / 2.0f, 1.0f);
            rotate_y(time);
            if(!draw_shape(SHAPE_QUAD)) return 1;

            // push_matrix();
            // translate(0.0f, 1.0f, 0.0f);
            // rotate_y(time);
            // if(!draw_shape(SHAPE_QUAD)) return 1;
            // pop_matrix();

        // draw_texture(texture, width / 2, height / 2, PINK);

        end_mode_3d();
        end_drawing();
    }

    unload_texture(tex);
    unload_texture(tex_2);
    close_window();
    return 0;
}
