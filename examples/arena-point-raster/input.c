typedef struct {
    size_t fps;
    float ms;
} Time_Record;

typedef struct {
    Time_Record *items;
    size_t count;
    size_t capacity;
    const size_t max;
    bool collecting;
} Time_Records;

static Shader_Mode shader_mode = SHADER_MODE_MODEL;
static Time_Records records = {.max = MAX_FPS_REC};
static int cam_move_idx = 0;
static int cam_view_idx = 0;
static Camera camera_defaults[NUM_CCTVS] = {0};

void copy_camera_infos(Camera *dst, const Camera *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
}

void save_camera_defaults(const Camera *src)
{
    copy_camera_infos(camera_defaults, src, NUM_CCTVS);
}

bool handle_input(Camera *cameras, size_t cam_count, bool *playing, size_t *lod, Vk_Buffer ubo_buff, Vk_Buffer depth_map_ubo_buff, Vk_Buffer frame_buff)
{
    update_camera_free(&cameras[cam_move_idx]);

    /* handle keyboard input */
    if (is_key_pressed(KEY_UP) || is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
        if (++(*lod) < LOD_COUNT) {
            wait_idle();
            if (!pc_layers[*lod].buff.handle) {
                if (!load_points(pc_layers[*lod].name, &pc_layers[*lod])) return false;

                /* upload new buffer and update descriptor sets */
                if (!vk_comp_buff_staged_upload(&pc_layers[*lod].buff, pc_layers[*lod].items)) return false;
                if (!update_render_ds_sets(ubo_buff, depth_map_ubo_buff, frame_buff, *lod))  return false;
            }

            log_point_count(pc_layers, *lod);

            /* rebuild compute commands */
            if (!build_compute_cmds(*lod)) return false;
        } else {
            vk_log(VK_INFO, "max lod %zu reached", --(*lod));
        }
    }
    if (is_key_pressed(KEY_DOWN) || is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
        if (*lod != 0) {
            log_point_count(pc_layers, *lod - 1);
            --(*lod);

            /* rebuild compute commands */
            wait_idle();
            if (!build_compute_cmds(*lod)) return false;
        } else {
            vk_log(VK_INFO, "min lod %zu reached", *lod);
        }
    }
    if (is_key_down(KEY_F)) log_fps();
    if (is_key_pressed(KEY_R) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        records.collecting = true;
    if ((is_key_pressed(KEY_M) && !is_key_down(KEY_LEFT_SHIFT)) ||
         is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
        shader_mode = (shader_mode + 1) % SHADER_MODE_COUNT;
        log_shader_mode(shader_mode);
    }
    if ((is_key_pressed(KEY_M) && is_key_down(KEY_LEFT_SHIFT)) ||
         is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1)) {
        shader_mode = (shader_mode - 1 + SHADER_MODE_COUNT) % SHADER_MODE_COUNT;
        log_shader_mode(shader_mode);
    }
    if (is_key_pressed(KEY_C)) {
        cam_move_idx = (cam_move_idx + 1) % cam_count;
        vk_log(VK_INFO, "piloting camera %d", cam_move_idx);
    }
    if (is_key_pressed(KEY_SPACE)) {
        vk_log(VK_INFO, "resetting camera defaults");
        copy_camera_infos(&cameras[1], camera_defaults, VK_ARRAY_LEN(camera_defaults));
        cam_move_idx = 0;
        cam_view_idx = 0;
    }
    if (is_key_pressed(KEY_P) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        *playing = !(*playing);
    if (is_key_pressed(KEY_L)) log_cameras(&cameras[1], NUM_CCTVS);
    if (is_key_pressed(KEY_V)) {
        /* Number of cctv cameras plus the main viewing camera */
        cam_view_idx = (cam_view_idx + 1) % (NUM_CCTVS + 1);
        cam_move_idx = cam_view_idx;
    }

    return true;
}
