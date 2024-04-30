#include "cvr.h"
#include "ext/nob.h"

int main()
{
    Camera camera = {
        .position   = {5.0f, 5.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    const int width  = 500;
    const int height = 500;
    init_window(width, height, "Load texture");

    const char *img_name = "res/statue.jpg";
    Image img = load_image(img_name);
    if (!img.data) {
        nob_log(NOB_ERROR, "image %s could not be loaded", img_name);
        return 1;
    } else {
        nob_log(NOB_INFO, "image %s was successfully loaded", img_name);
        nob_log(NOB_INFO, "    (height, width) = (%d, %d)", img.height, img.width);
        nob_log(NOB_INFO, "    image size = %d bytes", img.height * img.width * 4);
    }

    Texture tex = load_texture_from_image(img);
    (void)tex;
    unload_image(img);

    while(!window_should_close()) {
        begin_drawing(RED);
        begin_mode_3d(camera);

        // draw_texture(texture, width / 2, height / 2, PINK);

        end_mode_3d();
        end_drawing();
    }

    unload_texture(tex);
    close_window();
    return 0;
}
