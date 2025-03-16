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

static const char *video_names[] = {
    "suite_e_1280x960",
    "suite_w_1280x960",
    "suite_se_1280x720",
    "suite_nw_1280x720",
};

typedef struct {
    Vk_Texture planes[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    Vk_Buffer stg_buffs[VIDEO_IDX_COUNT * VIDEO_PLANE_COUNT];
    plm_t *plms[VIDEO_IDX_COUNT];
    float aspects[VIDEO_IDX_COUNT];
    plm_frame_t initial_frames[VIDEO_IDX_COUNT];
} Video_Textures;

static Video_Textures video_textures = {0};

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

static Video_Queue video_queue = {0};
static pthread_t prod_thread;

void *producer(void *arg);

void video_queue_init()
{
    pthread_mutex_init(&video_queue.mutex, NULL);
    pthread_cond_init(&video_queue.not_full, NULL);
    pthread_cond_init(&video_queue.not_empty, NULL);

    /* use a initial frame to determine memory requirements */
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

    pthread_create(&prod_thread, NULL, producer, NULL);
}

void video_queue_destroy()
{
    /* stops the producer thread */
    pthread_cancel(prod_thread);
    pthread_join(prod_thread, NULL);

    pthread_mutex_destroy(&video_queue.mutex);
    pthread_cond_destroy(&video_queue.not_full);
    pthread_cond_destroy(&video_queue.not_empty);

    for (size_t i = 0; i < MAX_QUEUED_FRAMES * VIDEO_IDX_COUNT; i++) {
        free(video_queue.frames[i].y.data);
        free(video_queue.frames[i].cb.data);
        free(video_queue.frames[i].cr.data);
    }
}

bool update_video_texture(void *data, Video_Idx vid_idx, Video_Plane_Type vid_plane_type);

bool video_dequeue()
{
    pthread_mutex_lock(&video_queue.mutex);
        /* if queue is empty then wait for producer */
        if (video_queue.size == 0)
            pthread_cond_wait(&video_queue.not_empty, &video_queue.mutex);

        /* dequeue the next four video frames */
        for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
            plm_frame_t *saved = &video_queue.frames[i + video_queue.tail * VIDEO_IDX_COUNT];
            if (!update_video_texture(saved->y.data, i, VIDEO_PLANE_Y))   return false;
            if (!update_video_texture(saved->cb.data, i, VIDEO_PLANE_CB)) return false;
            if (!update_video_texture(saved->cr.data, i, VIDEO_PLANE_CR)) return false;
        }
        video_queue.tail = (video_queue.tail + 1) % MAX_QUEUED_FRAMES;
        video_queue.size--;
        pthread_cond_signal(&video_queue.not_full);
    pthread_mutex_unlock(&video_queue.mutex);

    return true;
}

bool video_enqueue()
{
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
    vk_log(VK_INFO, "producer finished");
    return NULL;
}

bool update_video_texture(void *data, Video_Idx vid_idx, Video_Plane_Type vid_plane_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    memcpy(stg_buff->mapped, data, stg_buff->size);

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

bool init_video_texture(void *data, int width, int height, Video_Idx vid_idx, Video_Plane_Type vid_plane_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer *stg_buff = &video_textures.stg_buffs[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT];
    stg_buff->size  = width * height * format_to_size(VK_FORMAT_R8_UNORM);
    stg_buff->count = width * height;
    if (!vk_stg_buff_init(stg_buff, data, true)) return false;

    /* create the image */
    Vk_Image vk_img = {
        .extent  = {width, height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8_UNORM,
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
        vk_log(VK_ERROR, "failed to create image view");
        return false;
    };

    VkSampler sampler;
    if (!vk_sampler_init(&sampler)) return false;

    Vk_Texture plane = {
        .view = img_view,
        .sampler = sampler,
        .img = vk_img,
    };
    video_textures.planes[vid_plane_type + vid_idx * VIDEO_PLANE_COUNT] = plane;

    return true;
}

bool decode_initial_vid_frames(void)
{
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
        if (!init_video_texture(frame->y.data, frame->y.width, frame->y.height, i, VIDEO_PLANE_Y))     return false;
        if (!init_video_texture(frame->cb.data, frame->cb.width, frame->cb.height, i, VIDEO_PLANE_CB)) return false;
        if (!init_video_texture(frame->cr.data, frame->cr.width, frame->cr.height, i, VIDEO_PLANE_CR)) return false;
    }

    return true;
}

bool load_videos(void)
{
    for (size_t i = 0; i < VIDEO_IDX_COUNT; i++) {
        const char *file_name = temp_sprintf("res/%s.mpg", video_names[i]);
        plm_t *plm = plm_create_with_filename(file_name);
        if (!plm) {
            vk_log(VK_ERROR, "could not open file %s", file_name);
            return false;
        } else {
            video_textures.plms[i] = plm;
            plm_set_audio_enabled(plm, FALSE);
            int width  = plm_get_width(plm);
            int height = plm_get_height(plm);
            video_textures.aspects[i] = (float)width / height;
        }
    }

    return true;
}
