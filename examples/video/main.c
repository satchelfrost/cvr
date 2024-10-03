#include "vk_ctx.h"
#include "cvr.h"
#include "ext/nob.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#define FACTOR 50
#define WIDTH  (16 * FACTOR)
#define HEIGHT (9 * FACTOR)

typedef enum {
    VIDEO_PLANE_Y,
    VIDEO_PLANE_CB,
    VIDEO_PLANE_CR,
    VIDEO_PLANE_COUNT,
} Video_Plane_Type;

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
    Vk_Texture planes[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    Vk_Buffer stg_buffs[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    VkDescriptorSet ds_sets[VIDEO_IDX_COUNT];
    VkDescriptorPool ds_pool;
    VkDescriptorSetLayout ds_layout;
    VkPipelineLayout pl_layout;
    VkPipeline gfx_pl;
} Video_Textures;

Video_Textures video_textures = {0};
 
bool setup_ds_layout()
{
    VkDescriptorSetLayoutBinding tex_frag_binding = {DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)};
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &tex_frag_binding,
    };
    if (!vk_create_ds_layout(layout_ci, &video_textures.ds_layout)) return false;

    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes = {DS_POOL(COMBINED_IMAGE_SAMPLER, 1)};
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_sizes,
        // .maxSets = VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT,
        .maxSets = 1, // TODO: here for testing only
    };

    if (!vk_create_ds_pool(pool_ci, &video_textures.ds_pool)) return false;

    return true;
}

bool setup_ds_sets()
{
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(&video_textures.ds_layout, 1, video_textures.ds_pool)};
    if (!vk_alloc_ds(alloc, video_textures.ds_sets)) return false;

    VkDescriptorImageInfo img_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView   = video_textures.planes[0].view,
        .sampler     = video_textures.planes[0].sampler,
    };
    VkWriteDescriptorSet writes[] = {
        {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, video_textures.ds_sets[VIDEO_IDX_SUITE_E], &img_info)},
    };
    vk_update_ds(NOB_ARRAY_LEN(writes), writes);

    return true;
}

bool init_video_texture(Image img, Video_Idx vid_idx, Video_Plane_Type vid_plane_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
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

    Vk_Texture plane = {
        .view = img_view,
        .sampler = sampler,
        .img = vk_img,
        .id = vid_plane_type + vid_idx * VIDEO_PLANE_COUNT,
        .active = true,
    };
    video_textures.planes[plane.id] = plane;

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
    if (!init_video_texture(img, VIDEO_IDX_SUITE_E, VIDEO_PLANE_Y)) return 1;
    unload_image(img);

    /* setup descriptors */
    if (!setup_ds_layout()) return 1;
    if (!setup_ds_pool())   return 1;
    if (!setup_ds_sets())   return 1;

    /* setup the graphics pipeline */
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float16)};
    if (!vk_pl_layout_init2(video_textures.ds_layout, &video_textures.pl_layout, &pk_range, 1)) return 1;
    const char *shaders[] = {"./res/default.vert.spv", "./res/default.frag.spv"};
    if (!vk_basic_pl_init2(video_textures.pl_layout, shaders[0], shaders[1], &video_textures.gfx_pl)) return 1;
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
        for (size_t j = 0; j < VIDEO_PLANE_COUNT; j++) {
            vk_unload_texture2(video_textures.planes[j + i * VIDEO_PLANE_COUNT]);
        }
    }
    close_window();
    return 0;
}
