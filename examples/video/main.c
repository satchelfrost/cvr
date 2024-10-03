#include "vk_ctx.h"
#include "cvr.h"
#include "ext/nob.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#define FACTOR 50
#define WIDTH  (16 * FACTOR)
#define HEIGHT (9 * FACTOR)

typedef enum {
    VIDEO_TEX_TYPE_Y,
    VIDEO_TEX_TYPE_CB,
    VIDEO_TEX_TYPE_CR,
    VIDEO_TEX_COUNT,
} Video_Tex_Type;

typedef enum {
    VIDEO_IDX_SUITE_E,
    VIDEO_IDX_SUITE_NW,
    VIDEO_IDX_SUITE_SE,
    VIDEO_IDX_SUITE_W,
    VIDEO_IDX_COUNT,
} Video_Idx;

const char *video_names[] = {
    "suite_e",
    "suite_nw",
    "suite_se",
    "suite_w",
};

typedef struct {
    Vk_Texture textures[VIDEO_TEX_COUNT];
    Vk_Buffer stg_buffs[VIDEO_TEX_COUNT];
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
} Video_Texture;

Video_Texture video_textures[VIDEO_IDX_COUNT] = {0};

bool init_video_texture(Image img, Video_Tex_Type vid_tex_type, Video_Idx vid_idx)
{
    bool result = true;

    if (vid_tex_type < 0 || vid_tex_type >= VIDEO_TEX_COUNT) {
        nob_log(NOB_ERROR, "only three textures are required for ycbcr");
        return false;
    }

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures[vid_idx].stg_buffs[vid_tex_type];
    stg_buff->size  = img.width * img.height * format_to_size(img.format);
    stg_buff->count = img.width * img.height;

    if (!vk_stg_buff_init(stg_buff, img.data)) return false;

    /* create the image */
    Vk_Image vk_img = {
        .extent  = {img.width, img.height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = img.format,
    };
    result = vk_img_init(
        &vk_img,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (!result) nob_return_defer(false);

    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    vk_img_copy(vk_img.handle, stg_buff->handle, vk_img.extent);
    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    /* create image view */
    VkImageView img_view;
    if (!vk_img_view_init(vk_img, &img_view)) {
        nob_log(NOB_ERROR, "failed to create image view");
        nob_return_defer(false);
    };

    VkSampler sampler;
    if (!vk_sampler_init(&sampler)) nob_return_defer(false);

    Vk_Texture texture = {
        .view = img_view,
        .sampler = sampler,
        .img = vk_img,
        .id = vid_tex_type + vid_idx * VIDEO_TEX_COUNT,
        .active = true,
    };
    video_textures[vid_idx].textures[vid_tex_type] = texture;

defer:
    vk_buff_destroy(*stg_buff);
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
    plm_set_audio_enabled(plm, FALSE);
    Image img = {
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .width  = plm_get_width(plm),
        .height = plm_get_height(plm),
    };
    float aspect = (float)img.width / img.height;

    nob_log(NOB_INFO, "first frame %s was successfully loaded from", file_name);
    nob_log(NOB_INFO, "    (height, width) = (%d, %d)", img.height, img.width);
    nob_log(NOB_INFO, "    image size in memory = %d bytes", img.height * img.width * 4);

    /* decode one frame and use the luminance to create a texture */
    plm_frame_t *frame =  plm_decode_video(plm);
    img.data = frame->y.data;
    if (!init_video_texture(img, VIDEO_TEX_TYPE_Y, VIDEO_IDX_SUITE_E)) return 1;
    unload_image(img);

    return 1;

    while(!window_should_close()) {
        begin_drawing(BLUE);
            begin_mode_3d(camera);
                scale(aspect, 1.0f, 1.0f);
                // if (!draw_texture(tex, SHAPE_QUAD)) return 1;
            end_mode_3d();
        end_drawing();
    }

    /* cleanup */
    plm_destroy(plm);
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        for (size_t j = 0; j < VIDEO_TEX_COUNT; j++) {
            vk_unload_texture2(video_textures[i].textures[j]);
        }
    }
    close_window();
    return 0;
}
