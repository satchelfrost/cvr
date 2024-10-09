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
    VIDEO_IDX_SUITE_W,
    VIDEO_IDX_SUITE_NW,
    VIDEO_IDX_SUITE_SE,
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
    Image img[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    plm_t *plms[VIDEO_IDX_COUNT];
    float aspects[VIDEO_IDX_COUNT];
    VkDescriptorSet ds_sets[VIDEO_IDX_COUNT];
    VkDescriptorSetLayout ds_layout;
    VkDescriptorPool ds_pool;
    VkPipelineLayout pl_layout;
    VkPipeline gfx_pl;
} Video_Textures;

Video_Textures video_textures = {0};
 
bool setup_ds_layout()
{
    VkDescriptorSetLayoutBinding tex_frag_bindings[] = {
        {DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)},
        {DS_BINDING(1, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)},
        {DS_BINDING(2, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)},
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = NOB_ARRAY_LEN(tex_frag_bindings),
        .pBindings = tex_frag_bindings,
    };
    if (!vk_create_ds_layout(layout_ci, &video_textures.ds_layout)) return false;

    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes = {
        DS_POOL(COMBINED_IMAGE_SAMPLER, VIDEO_PLANE_COUNT * VIDEO_IDX_COUNT)
    };
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_sizes,
        .maxSets = VIDEO_IDX_COUNT,
    };

    if (!vk_create_ds_pool(pool_ci, &video_textures.ds_pool)) return false;

    return true;
}

bool setup_ds_sets()
{
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(&video_textures.ds_layout, 1, video_textures.ds_pool)};
        if (!vk_alloc_ds(alloc, &video_textures.ds_sets[i])) return false;

        VkDescriptorImageInfo y_img_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView  = video_textures.planes[VIDEO_PLANE_Y + i * VIDEO_PLANE_COUNT].view,
            .sampler    = video_textures.planes[VIDEO_PLANE_Y + i * VIDEO_PLANE_COUNT].sampler,
        };
        VkDescriptorImageInfo cb_img_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = video_textures.planes[VIDEO_PLANE_CB + i * VIDEO_PLANE_COUNT].view,
            .sampler   = video_textures.planes[VIDEO_PLANE_CB + i * VIDEO_PLANE_COUNT].sampler,
        };
        VkDescriptorImageInfo cr_img_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = video_textures.planes[VIDEO_PLANE_CR + i * VIDEO_PLANE_COUNT].view,
            .sampler   = video_textures.planes[VIDEO_PLANE_CR + i * VIDEO_PLANE_COUNT].sampler,
        };
        VkWriteDescriptorSet writes[] = {
            {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, video_textures.ds_sets[i], &y_img_info)},
            {DS_WRITE_IMG(1, COMBINED_IMAGE_SAMPLER, video_textures.ds_sets[i], &cb_img_info)},
            {DS_WRITE_IMG(2, COMBINED_IMAGE_SAMPLER, video_textures.ds_sets[i], &cr_img_info)},
        };
        vk_update_ds(NOB_ARRAY_LEN(writes), writes);
    }

    return true;
}

// TODO: I should be working with plm_frame_t instead of Image because image is meant to be used with cvr.h API
bool init_video_texture(Image img, Video_Idx vid_idx, Video_Plane_Type vid_plane_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    stg_buff->size  = img.width * img.height * format_to_size(img.format);
    stg_buff->count = img.width * img.height;
    if (!vk_stg_buff_init(stg_buff, img.data, true)) return false;

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
    if (!result) return false;

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
        return false;
    };

    VkSampler sampler;
    if (!vk_sampler_init(&sampler)) return false;

    Vk_Texture plane = {
        .view = img_view,
        .sampler = sampler,
        .img = vk_img,
        .id = vid_plane_type + vid_idx * VIDEO_PLANE_COUNT,
    };
    video_textures.planes[plane.id] = plane;

    return true;
}

bool update_video_texture(Image img, Video_Idx vid_idx, Video_Plane_Type vid_plane_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    memcpy(stg_buff->mapped, img.data, stg_buff->size);

    Vk_Image vk_img = video_textures.planes[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT].img;
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

    return result;
}

int main()
{
    Camera camera = {
        .position   = {0.0f, 0.0f, 2.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45,
        .projection = PERSPECTIVE,
    };

    init_window(WIDTH, HEIGHT, "video");
    // set_target_fps(60);

    /* create a texture from video */
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        const char *file_name = nob_temp_sprintf("res/%s_snippet.mpg", video_names[i]);
        // const char *file_name = "res/bjork-all-is-full-of-love.mpg";
        plm_t *plm = plm_create_with_filename(file_name);
        if (!plm) {
            nob_log(NOB_ERROR, "could not open file %s", file_name);
            return 1;
        } else {
            video_textures.plms[i] = plm;
            // plm_set_loop(plm, TRUE);
            plm_set_audio_enabled(plm, FALSE);
            Image img = {
                .format = VK_FORMAT_R8_UNORM,
                .width  = plm_get_width(plm),
                .height = plm_get_height(plm),
            };
            video_textures.aspects[i] = (float)img.width / img.height;
            nob_log(NOB_INFO, "first frame %s was successfully loaded from", file_name);
            nob_log(NOB_INFO, "    (height, width) = (%d, %d)", img.height, img.width);
            nob_log(NOB_INFO, "    image size in memory = %d bytes", img.height * img.width * 4);
        }
    }

    /* decode one frame and use the luminance to create a texture */
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        Image img = {.format = VK_FORMAT_R8_UNORM};
        plm_frame_t *frame = plm_decode_video(video_textures.plms[i]);
        img.data   = frame->y.data;
        img.width  = frame->y.width;
        img.height = frame->y.height;
        nob_log(NOB_INFO, "y height %zu width %zu", frame->y.width, frame->y.height);
        if (!init_video_texture(img, i, VIDEO_PLANE_Y)) return 1;
        img.data   = frame->cb.data;
        img.width  = frame->cb.width;
        img.height = frame->cb.height;
        nob_log(NOB_INFO, "cb height %zu width %zu", frame->cb.width, frame->cb.height);
        if (!init_video_texture(img, i, VIDEO_PLANE_CB)) return 1;
        img.data   = frame->cr.data;
        img.width  = frame->cr.width;
        img.height = frame->cr.height;
        nob_log(NOB_INFO, "cr height %zu width %zu", frame->cr.width, frame->cr.height);
        if (!init_video_texture(img, i, VIDEO_PLANE_CR)) return 1;
    }

    /* setup descriptors */
    if (!setup_ds_layout()) return 1;
    if (!setup_ds_pool())   return 1;
    if (!setup_ds_sets())   return 1;

    /* setup the graphics pipeline */
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float16)};
    if (!vk_pl_layout_init2(video_textures.ds_layout, &video_textures.pl_layout, &pk_range, 1)) return 1;
    const char *shaders[] = {"./res/texture.vert.spv", "./res/texture.frag.spv"};
    if (!vk_basic_pl_init2(video_textures.pl_layout, shaders[0], shaders[1], &video_textures.gfx_pl)) return 1;

    float vid_update_time = 0.0f;
    bool playback_finished = false;

    while(!window_should_close() && !playback_finished) {
        /* update */
        log_fps();
        update_camera_free(&camera);
        vid_update_time += get_frame_time();

        /* decode the next frame */
        if (vid_update_time > 1.0f / 30.0f) {
            for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
                plm_frame_t *frame = plm_decode_video(video_textures.plms[i]);
                if (!frame) {
                    playback_finished = true;
                    nob_log(NOB_INFO, "playback finished!");
                    return 1;
                }

                Image img = {.format = VK_FORMAT_R8_UNORM};
                img.data   = frame->y.data;
                img.width  = frame->y.width;
                img.height = frame->y.height;
                if (!update_video_texture(img, i, VIDEO_PLANE_Y)) return 1;
                img.data   = frame->cb.data;
                img.width  = frame->cb.width;
                img.height = frame->cb.height;
                if (!update_video_texture(img, i, VIDEO_PLANE_CB)) return 1;
                img.data   = frame->cr.data;
                img.width  = frame->cr.width;
                img.height = frame->cr.height;
                if (!update_video_texture(img, i, VIDEO_PLANE_CR)) return 1;
            }

            vid_update_time = 0.0f;
        }

        /* drawing */
        begin_drawing(BLUE);
            begin_mode_3d(camera);
                for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
                    push_matrix();
                    scale(video_textures.aspects[i], 1.0f, 1.0f);
                    translate(i - 1.0f, 0.0f, 0.0f);
                    if (!draw(video_textures.gfx_pl, video_textures.pl_layout, video_textures.ds_sets[i], SHAPE_QUAD))
                        return 1;
                    pop_matrix();
                }
            end_mode_3d();
        end_drawing();
    }

    /* cleanup device */
    wait_idle();
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        plm_destroy(video_textures.plms[i]);
        for (size_t j = 0; j < VIDEO_PLANE_COUNT; j++) {
            vk_buff_destroy(video_textures.stg_buffs[j + i * VIDEO_PLANE_COUNT]);
            vk_unload_texture2(video_textures.planes[j + i * VIDEO_PLANE_COUNT]);
        }
    }
    vk_destroy_ds_layout(video_textures.ds_layout);
    vk_destroy_ds_pool(video_textures.ds_pool);
    vk_destroy_pl_res(video_textures.gfx_pl, video_textures.pl_layout);

    close_window();
    return 0;
}
