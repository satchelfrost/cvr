/*
 * This example decodes four videos on one thread,
 * and renders them on a seperate thread. The result
 * gets displayed on four quads.
 *
 * assets:
 * https://phoboslab.org/files/bjork-all-is-full-of-love.mpg
 * */

#include "cvr.h"
#include <pthread.h>
#include "geometry.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h" // https://github.com/phoboslab/pl_mpeg

#define NOB_IMPLEMENTATION
#include "../../nob.h"

#define FACTOR 50
#define WIDTH  (16 * FACTOR)
#define HEIGHT (9 * FACTOR)

/* This is how many videos the example will try to decode.
 * for one video, multi threading probably isn't necessary */
#define VIDEO_IDX_COUNT 1

/* Here we have a dual-threaded approach. One thread is constantly decoding and populating
 * a queue, the other thread is dequeuing and drawing that texture to a quad.
 * */
// #define DUAL_THREADED        // disabling reverts to a single thread

/* In the dual-threaded approach we have the classic producer-consumer problem.
 * The queue print is a simple ascii visualization of who is winning:
 * the decoding thread (producer), or drawing thread (consumer)*/
// #define DEBUG_QUEUE_PRINT // disabling doesn't print the video queue

typedef enum {
    VIDEO_PLANE_Y,
    VIDEO_PLANE_CB,
    VIDEO_PLANE_CR,
    VIDEO_PLANE_COUNT,
} Video_Plane_Type;


typedef struct {
    Rvk_Texture planes[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    Rvk_Buffer stg_buffs[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    plm_t *plms[VIDEO_IDX_COUNT];
    float aspects[VIDEO_IDX_COUNT];
    plm_frame_t initial_frames[VIDEO_IDX_COUNT];
    VkDescriptorSet ds_sets[VIDEO_IDX_COUNT];
    Rvk_Descriptor_Set_Layout ds_layout;
    VkDescriptorPool ds_pool;
    VkPipelineLayout pl_layout;
    VkPipeline gfx_pl;
} Video_Textures;

Video_Textures video_textures = {0};

#define MAX_QUEUED_FRAMES 20
typedef struct {
    plm_frame_t frames[VIDEO_IDX_COUNT * MAX_QUEUED_FRAMES];
    int head;
    int tail;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} Video_Queue;

Video_Queue video_queue = {0};
Rvk_Descriptor_Pool_Arena arena = {0};

void update_video_texture(void *data, size_t vid_idx, Video_Plane_Type vid_plane_type);

void video_queue_init()
{
    pthread_mutex_init(&video_queue.mutex, NULL);
    pthread_cond_init(&video_queue.not_full, NULL);
    pthread_cond_init(&video_queue.not_empty, NULL);

    /* use a single frame to determine how much memory needs allocated for a decoded frame */
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        for (size_t j = 0; j < MAX_QUEUED_FRAMES; j++) {
            plm_frame_t *frame = &video_textures.initial_frames[i];
            video_queue.frames[i + j * VIDEO_IDX_COUNT] = *frame; // shallow copy first
            size_t y_size  = frame->y.width  * frame->y.height;
            size_t cb_size = frame->cb.width * frame->cb.height;
            size_t cr_size = frame->cr.width * frame->cr.height;
            video_queue.frames[i + j * VIDEO_IDX_COUNT].y.data  = malloc(y_size);
            video_queue.frames[i + j * VIDEO_IDX_COUNT].cb.data = malloc(cb_size);
            video_queue.frames[i + j * VIDEO_IDX_COUNT].cr.data = malloc(cr_size);
        }
    }
}

void video_queue_destroy()
{
    pthread_mutex_destroy(&video_queue.mutex);
    pthread_cond_destroy(&video_queue.not_full);
    pthread_cond_destroy(&video_queue.not_empty);

    for (size_t i = 0; i < MAX_QUEUED_FRAMES * VIDEO_IDX_COUNT; i++) {
        free(video_queue.frames[i].y.data);
        free(video_queue.frames[i].cb.data);
        free(video_queue.frames[i].cr.data);
    }
}

bool video_enqueue()
{
#ifdef DEBUG_QUEUE_PRINT
    char queue_str[MAX_QUEUED_FRAMES + 3];
    queue_str[0] = '[';
    queue_str[MAX_QUEUED_FRAMES + 1] = ']';
    queue_str[MAX_QUEUED_FRAMES + 2] = 0;
    for (int i = 0; i < MAX_QUEUED_FRAMES; i++)
        queue_str[i + 1] = (i < video_queue.size) ? '*' : ' ';
    rvk_log(RVK_INFO, "%s", queue_str);
#endif

    /* first decode the frames */
    plm_frame_t temp_frames[VIDEO_IDX_COUNT];
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        plm_frame_t *frame = plm_decode_video(video_textures.plms[i]);
        if (!frame) return false;

        size_t y_size  = frame->y.width  * frame->y.height;
        size_t cb_size = frame->cb.width * frame->cb.height;
        size_t cr_size = frame->cr.width * frame->cr.height;
        temp_frames[i] = *frame; // shallow copy first
        memcpy(temp_frames[i].y.data , frame->y.data,  y_size);
        memcpy(temp_frames[i].cb.data, frame->cb.data, cb_size);
        memcpy(temp_frames[i].cr.data, frame->cr.data, cr_size);
    }

    pthread_mutex_lock(&video_queue.mutex);
        /* if queue is full then wait for consumer */
        if (video_queue.size == MAX_QUEUED_FRAMES)
            pthread_cond_wait(&video_queue.not_full, &video_queue.mutex);

        /* decode the next "VIDEO_IDX_COUNT" video frames and enqueue */
        for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
            plm_frame_t *saved = &video_queue.frames[i + video_queue.head * VIDEO_IDX_COUNT];
            size_t y_size  = temp_frames[i].y.width  * temp_frames[i].y.height;
            size_t cb_size = temp_frames[i].cb.width * temp_frames[i].cb.height;
            size_t cr_size = temp_frames[i].cr.width * temp_frames[i].cr.height;
            memcpy(saved->y.data , temp_frames[i].y.data,  y_size);
            memcpy(saved->cb.data, temp_frames[i].cb.data, cb_size);
            memcpy(saved->cr.data, temp_frames[i].cr.data, cr_size);
        }
        video_queue.head = (video_queue.head + 1) % MAX_QUEUED_FRAMES;
        video_queue.size++;
        pthread_cond_signal(&video_queue.not_empty);
    pthread_mutex_unlock(&video_queue.mutex);

    return true;
}

bool video_dequeue()
{
    pthread_mutex_lock(&video_queue.mutex);
        /* if queue is empty then wait for producer */
        if (video_queue.size == 0)
            pthread_cond_wait(&video_queue.not_empty, &video_queue.mutex);

        /* dequeue the next four video frames */
        for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
            plm_frame_t *saved = &video_queue.frames[i + video_queue.tail * VIDEO_IDX_COUNT];
            update_video_texture(saved->y.data, i, VIDEO_PLANE_Y);
            update_video_texture(saved->cb.data, i, VIDEO_PLANE_CB);
            update_video_texture(saved->cr.data, i, VIDEO_PLANE_CR);
        }
        video_queue.tail = (video_queue.tail + 1) % MAX_QUEUED_FRAMES;
        video_queue.size--;
        pthread_cond_signal(&video_queue.not_full);
    pthread_mutex_unlock(&video_queue.mutex);

    return true;
}

void *producer(void *arg)
{
    (void)arg;
    while (true) {
        if (!video_enqueue()) {
            /* loop all videos even if just one has finished */
            for (size_t i = 0; i < VIDEO_IDX_COUNT; i++)
                plm_rewind(video_textures.plms[i]);
        }
    };
    rvk_log(RVK_INFO, "producer finished");
    return NULL;
}
 
void setup_ds_layout()
{
    VkDescriptorSetLayoutBinding tex_frag_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };
    rvk_ds_layout_init(tex_frag_bindings, RVK_ARRAY_LEN(tex_frag_bindings), &video_textures.ds_layout);
}

void setup_ds_sets()
{
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        rvk_descriptor_pool_arena_alloc_set(&arena, &video_textures.ds_layout, &video_textures.ds_sets[i]);

        VkDescriptorImageInfo *y_img_info  = &video_textures.planes[VIDEO_PLANE_Y + i * VIDEO_PLANE_COUNT].info;
        VkDescriptorImageInfo *cb_img_info = &video_textures.planes[VIDEO_PLANE_CB + i * VIDEO_PLANE_COUNT].info;
        VkDescriptorImageInfo *cr_img_info = &video_textures.planes[VIDEO_PLANE_CR + i * VIDEO_PLANE_COUNT].info;
        VkWriteDescriptorSet writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .dstSet = video_textures.ds_sets[i],
                .pImageInfo = y_img_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .dstSet = video_textures.ds_sets[i],
                .pImageInfo = cb_img_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .dstSet = video_textures.ds_sets[i],
                .pImageInfo = cr_img_info,
            },
        };
        rvk_update_ds(RVK_ARRAY_LEN(writes), writes);
    }
}

void init_video_texture(void *data, int width, int height, size_t vid_idx, Video_Plane_Type vid_plane_type)
{
    /* create staging buffer for image */
    Rvk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    size_t size  = width * height * rvk_format_to_size(VK_FORMAT_R8_UNORM);
    size_t count = width * height;
    rvk_stage_buff_init(size, count, data, stg_buff);
    rvk_buff_map(stg_buff);

    /* create the image */
    Rvk_Image rvk_img = {
        .extent  = {width, height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8_UNORM,
    };
    rvk_img_init(
        &rvk_img,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    rvk_transition_img_layout(
        rvk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    rvk_img_copy(rvk_img.handle, stg_buff->handle, rvk_img.extent);
    rvk_transition_img_layout(
        rvk_img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    /* create image view */
    VkImageView img_view;
    rvk_img_view_init(rvk_img, &img_view);

    VkSampler sampler;
    rvk_sampler_init(&sampler);

    Rvk_Texture plane = {
        .view = img_view,
        .sampler = sampler,
        .img = rvk_img,
        .info = {
            .sampler = sampler,
            .imageView = img_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
    video_textures.planes[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT] = plane;
}

void update_video_texture(void *data, size_t vid_idx, Video_Plane_Type vid_plane_type)
{
    /* create staging buffer for image */
    Rvk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    memcpy(stg_buff->mapped, data, stg_buff->size);

    Rvk_Image rvk_img = video_textures.planes[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT].img;
    rvk_transition_img_layout(
        rvk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    rvk_img_copy(rvk_img.handle, stg_buff->handle, rvk_img.extent);
    rvk_transition_img_layout(
        rvk_img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void create_pipeline()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float16)};
    rvk_create_pipeline_layout(
        &video_textures.pl_layout,
        .p_push_constant_ranges = &pk_range,
        .p_set_layouts = &video_textures.ds_layout.handle,
    );

    /* create pipeline */
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, pos),
        },
        {
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color),
        },
        {
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, tex_coord),
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vertex),
    };
    Pipeline_Config config = {
        .pl_layout = video_textures.pl_layout,
        .vert = "./res/texture.vert.glsl.spv",
        .frag = "./res/texture.frag.glsl.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = RVK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    rvk_basic_pl_init(config, &video_textures.gfx_pl);
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
    set_target_fps(60);

    /* load videos into plm */
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        const char *file_name = "res/bjork-all-is-full-of-love.mpg";
        plm_t *plm = plm_create_with_filename(file_name);
        if (!plm) {
            rvk_log(RVK_ERROR, "could not open file %s", file_name);
            rvk_log(RVK_ERROR, "trying downloading https://phoboslab.org/files/bjork-all-is-full-of-love.mpg");
            return 1;
        } else {
            video_textures.plms[i] = plm;
            plm_set_audio_enabled(plm, FALSE);
            int width  = plm_get_width(plm);
            int height = plm_get_height(plm);
            video_textures.aspects[i] = (float)width / height;
        }
    }

    /* decode one frame initialize video textures */
    plm_frame_t *frame = NULL;
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        frame = plm_decode_video(video_textures.plms[i]);
        size_t y_size  = frame->y.width  * frame->y.height;
        size_t cb_size = frame->cb.width * frame->cb.height;
        size_t cr_size = frame->cr.width * frame->cr.height;
        video_textures.initial_frames[i] = *frame; // do a shallow copy first
        video_textures.initial_frames[i].y.data  = malloc(y_size);
        video_textures.initial_frames[i].cb.data = malloc(cb_size);
        video_textures.initial_frames[i].cr.data = malloc(cr_size);
        rvk_log(RVK_INFO, "y height %zu width %zu", frame->y.width, frame->y.height);
        rvk_log(RVK_INFO, "cb height %zu width %zu", frame->cb.width, frame->cb.height);
        rvk_log(RVK_INFO, "cr height %zu width %zu", frame->cr.width, frame->cr.height);
        init_video_texture(frame->y.data, frame->y.width, frame->y.height, i, VIDEO_PLANE_Y);
        init_video_texture(frame->cb.data, frame->cb.width, frame->cb.height, i, VIDEO_PLANE_CB);
        init_video_texture(frame->cr.data, frame->cr.width, frame->cr.height, i, VIDEO_PLANE_CR);
    }

    /* setup descriptors */
    rvk_descriptor_pool_arena_init(&arena);
    setup_ds_layout();
    setup_ds_sets();

#ifdef DUAL_THREADED
    video_queue_init();
    pthread_t prod_thread;
    pthread_create(&prod_thread, NULL, producer, NULL);
#endif

    create_pipeline();

    float vid_update_time = 0.0f;
    bool playback_finished = false;
    bool playing = true;

    while(!window_should_close() && !playback_finished) {
        /* update */
        log_fps();
        update_camera_free(&camera);

        if (is_key_pressed(KEY_P)) playing = !playing;

        if (playing) vid_update_time += get_frame_time();

        /* decode the next frame */
        if (vid_update_time > 1.0f / 30.0f && playing) {
#ifdef DUAL_THREADED
            if (video_dequeue()) {
                vid_update_time = 0.0f;
            } else {
                playback_finished = true;
                rvk_log(RVK_INFO, "video could not dequeue, this shouldn't happen");
                return 1;
            }
#else // On the fly decode, i.e. decode now on this thread
            for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
                plm_frame_t *frame = plm_decode_video(video_textures.plms[i]);
                if (!frame) {
                    playback_finished = true;
                    rvk_log(RVK_INFO, "playback finished!");
                    return 1;
                }

                update_video_texture(frame->y.data, i, VIDEO_PLANE_Y);
                update_video_texture(frame->cb.data, i, VIDEO_PLANE_CB);
                update_video_texture(frame->cr.data, i, VIDEO_PLANE_CR);
            }
            vid_update_time = 0.0f;
#endif
        }

        /* drawing */
        begin_drawing(BLUE);
            begin_mode_3d(camera);
                for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
                    push_matrix();
                    scale(video_textures.aspects[i], 1.0f, 1.0f);
                    // translate(0.0f, i - 1.0f, 0.0f);
                    draw_shape_ex(video_textures.gfx_pl, video_textures.pl_layout, video_textures.ds_sets[i], SHAPE_QUAD);
                    pop_matrix();
                }
            end_mode_3d();
        end_drawing();
    }

#ifdef DUAL_THREADED
    pthread_cancel(prod_thread);
    pthread_join(prod_thread, NULL);
    video_queue_destroy();
#endif

    /* cleanup device */
    rvk_wait_idle();
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        plm_destroy(video_textures.plms[i]);
        free(video_textures.initial_frames[i].y.data);
        free(video_textures.initial_frames[i].cb.data);
        free(video_textures.initial_frames[i].cr.data);
        for (size_t j = 0; j < VIDEO_PLANE_COUNT; j++) {
            rvk_buff_destroy(video_textures.stg_buffs[j + i * VIDEO_PLANE_COUNT]);
            rvk_unload_texture(video_textures.planes[j + i * VIDEO_PLANE_COUNT]);
        }
    }
    rvk_destroy_descriptor_set_layout(video_textures.ds_layout.handle);
    rvk_destroy_ds_pool(video_textures.ds_pool);
    rvk_destroy_pl_res(video_textures.gfx_pl, video_textures.pl_layout);
    rvk_descriptor_pool_arena_destroy(arena);

    close_window();
    return 0;
}
