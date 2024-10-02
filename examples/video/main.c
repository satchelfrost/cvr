#include "cvr.h"
#include "ext/nob.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#define FACTOR 50
#define WIDTH  (16 * FACTOR)
#define HEIGHT (9 * FACTOR)

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
        .position = {0.0f, 0.0f, 2.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45,
        .projection = PERSPECTIVE,
    };

    init_window(WIDTH, HEIGHT, "video");
    // set_target_fps(60);

    float time = 0.0f;
    Buffer buff = {
        .size  = sizeof(float),
        .count = 1,
        .items = &time,
    };
    if (!ubo_init(buff, EXAMPLE_TEX)) return 1;

    /* create a texture from video */
    const char *file_name = "res/suite_e_snippet.mpg";
	plm_t *plm = plm_create_with_filename(file_name);
	if (!plm) {
		nob_log(NOB_ERROR, "could not open file %s", file_name);
		return 1;
	}
    Image image = {.format = 43};
	plm_set_audio_enabled(plm, FALSE);
	image.width = plm_get_width(plm);
	image.height = plm_get_height(plm);

    nob_log(NOB_INFO, "image %s was successfully loaded", file_name);
    nob_log(NOB_INFO, "    (height, width) = (%d, %d)", image.height, image.width);
    nob_log(NOB_INFO, "    image size in memory = %d bytes", image.height * image.width * 4);

	uint8_t *rgba_buffer = (uint8_t *)malloc(image.width * image.height * 4);
    image.data = rgba_buffer;
	plm_frame_t *frame = plm_decode_video(plm);
    plm_frame_to_rgba(frame, rgba_buffer, image.width * 4);
    Texture tex = load_texture_from_image(image);
    float aspect = (float)tex.width / tex.height;

    // Texture tex = {0};
    // if (!load_texture("res/matrix.png", &tex)) return 1;
    // float aspect = (float)tex.width / tex.height;

    while(!window_should_close()) {
        // log_fps();
        begin_drawing(BLUE);
            begin_mode_3d(camera);
                // rotate_y(get_time());
                scale(aspect, 1.0f, 1.0f);
                if (!draw_texture(tex, SHAPE_QUAD)) return 1;
                // draw_shape(SHAPE_QUAD);
                // draw_shape(SHAPE_CUBE);
                
            end_mode_3d();
        end_drawing();
    }

    unload_texture(tex);
    close_window();
    return 0;
}
