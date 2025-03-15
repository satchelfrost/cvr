typedef struct {
    char *program;
    int x_pos;
    int y_pos;
    int width;
    int height;
    bool fullscreen;
    int highest_lod;
} Config;

typedef enum {
    SHADER_MODE_MODEL,
    SHADER_MODE_FRUSTUM_OVERLAP,
    SHADER_MODE_SINGLE_VID_TEX,
    SHADER_MODE_MULTI_VID_VIEW_SEAM_BLEND,
    SHADER_MODE_WIPE,
    SHADER_MODE_ANGLE_BLEND,
    SHADER_MODE_COUNT,
} Shader_Mode;

void log_shader_mode(Shader_Mode mode)
{
    switch (mode) {
    case SHADER_MODE_MODEL:
        vk_log(VK_INFO, "shader mode model"); break;
    case SHADER_MODE_FRUSTUM_OVERLAP:
        vk_log(VK_INFO, "shader mode frustum overlap"); break;
    case SHADER_MODE_SINGLE_VID_TEX:
        vk_log(VK_INFO, "shader mode single vid_tex"); break;
    case SHADER_MODE_MULTI_VID_VIEW_SEAM_BLEND:
        vk_log(VK_INFO, "shader mode multi vid view seam blend"); break;
    case SHADER_MODE_WIPE:
        vk_log(VK_INFO, "shader mode wipe"); break;
    case SHADER_MODE_ANGLE_BLEND:
        vk_log(VK_INFO, "shader mode angle blend"); break;
    default:
        vk_log(VK_ERROR, "Shader mode: unrecognized %d", mode);
        break;
    }
}

void log_usage(const char *program)
{
    vk_log(VK_INFO, "usage: %s <flags> <optional_input>", program);
    vk_log(VK_INFO, "    --help");
    vk_log(VK_INFO, "    --fullscreen");
    vk_log(VK_INFO, "    --log_controls");
    vk_log(VK_INFO, "    --width       <integer>");
    vk_log(VK_INFO, "    --height      <integer>");
    vk_log(VK_INFO, "    --x_pos       <integer>");
    vk_log(VK_INFO, "    --y_pos       <integer>");
    vk_log(VK_INFO, "    --highest_lod <integer> (renders all lods up to highest lod; [0, 6])");
}

void log_cameras(Camera *cameras, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        Vector3 pos = cameras[i].position;
        Vector3 up  = cameras[i].up;
        Vector3 tg  = cameras[i].target;
        float fov   = cameras[i].fovy;
        vk_log(VK_INFO, "Camera %d", i);
        vk_log(VK_INFO, "    position {%.2f, %.2f, %.2f}", pos.x, pos.y, pos.z);
        vk_log(VK_INFO, "    up       {%.2f, %.2f, %.2f}", up.x, up.y, up.z);
        vk_log(VK_INFO, "    target   {%.2f, %.2f, %.2f}", tg.x, tg.y, tg.z);
        vk_log(VK_INFO, "    fovy     %.2f", fov);
    }
}

void log_controls(void)
{
    vk_log(VK_INFO, "------------");
    vk_log(VK_INFO, "| Keyboard |");
    vk_log(VK_INFO, "------------");
    vk_log(VK_INFO, "    ------------");
    vk_log(VK_INFO, "    | Movement |");
    vk_log(VK_INFO, "    ------------");
    vk_log(VK_INFO, "        [W] - Forward");
    vk_log(VK_INFO, "        [A] - Left");
    vk_log(VK_INFO, "        [S] - Back");
    vk_log(VK_INFO, "        [D] - Right");
    vk_log(VK_INFO, "        [E] - Up");
    vk_log(VK_INFO, "        [Q] - Down");
    vk_log(VK_INFO, "        [Shift] - Fast movement");
    vk_log(VK_INFO, "        Right Click + Mouse Movement = Rotation");
    vk_log(VK_INFO, "    ------------");
    vk_log(VK_INFO, "    | Hot keys |");
    vk_log(VK_INFO, "    ------------");
    vk_log(VK_INFO, "        [M] - Next shader mode");
    vk_log(VK_INFO, "        [Shift + M] - Previous shader mode");
    vk_log(VK_INFO, "        [C] - Change piloted camera");
    vk_log(VK_INFO, "        [P] - Play/Pause");
    vk_log(VK_INFO, "        [R] - Record FPS");
    vk_log(VK_INFO, "        [P] - Print camera info");
    vk_log(VK_INFO, "        [Space] - Reset cameras to default position");
    vk_log(VK_INFO, "-----------");
    vk_log(VK_INFO, "| Gamepad |");
    vk_log(VK_INFO, "-----------");
    vk_log(VK_INFO, "    ------------");
    vk_log(VK_INFO, "    | Movement |");
    vk_log(VK_INFO, "    ------------");
    vk_log(VK_INFO, "        [Left Analog] - Translation");
    vk_log(VK_INFO, "        [Right Analog] - Rotation");
    vk_log(VK_INFO, "    ---------");
    vk_log(VK_INFO, "    | Other |");
    vk_log(VK_INFO, "    ---------");
    vk_log(VK_INFO, "        [Right Trigger] - Next shader mode");
    vk_log(VK_INFO, "        [Left Trigger] - Previous shader mode");
    vk_log(VK_INFO, "        [Right Face Right Button] - Play/Pause");
    vk_log(VK_INFO, "        [Right Face Down Button] - Record FPS");
    vk_log(VK_INFO, "        [D Pad Left] - decrease point cloud LOD");
    vk_log(VK_INFO, "        [D Pad Right] - increase point cloud LOD");
}

bool handle_usr_cmds(int argc, char **argv, Config *out_config)
{
    out_config->program = nob_shift_args(&argc, &argv);
    out_config->width = 1600;
    out_config->height = 900;
    while(argc > 0) {
        char *flag = nob_shift_args(&argc, &argv);
        if (strcmp("--fullscreen", flag) == 0) {
            out_config->fullscreen = true;
        } else if (strcmp("--width", flag) == 0) {
            if (argc == 0) {
                vk_log(VK_ERROR, "expected a width after '--width' flag");
                return false;
            } else {
                out_config->width = atoi(nob_shift_args(&argc, &argv));
            }
        } else if (strcmp("--height", flag) == 0) {
            if (argc == 0) {
                vk_log(VK_ERROR, "expected a width after '--height' flag");
                return false;
            } else {
                out_config->height = atoi(nob_shift_args(&argc, &argv));
            }
        } else if (strcmp("--x_pos", flag) == 0) {
            if (argc == 0) {
                vk_log(VK_ERROR, "expected a width after '--x_pos' flag");
                return false;
            } else {
                out_config->x_pos = atoi(nob_shift_args(&argc, &argv));
            }
        } else if (strcmp("--y_pos", flag) == 0) {
            if (argc == 0) {
                vk_log(VK_ERROR, "expected a width after '--x_pos' flag");
                return false;
            } else {
                out_config->y_pos = atoi(nob_shift_args(&argc, &argv));
            }
        } else if (strcmp("--highest_lod", flag) == 0) {
            if (argc == 0) {
                vk_log(VK_ERROR, "expected a width after '--highest_lod' flag");
                return false;
            } else {
                out_config->highest_lod = atoi(nob_shift_args(&argc, &argv));
                if (out_config->highest_lod < 0 || out_config->highest_lod >= LOD_COUNT) {
                    vk_log(VK_ERROR, "valid lods are between 0 and 6 inclusive");
                    return false;
                }
            }
        } else if (strcmp("--help", flag) == 0) {
            return false;
        } else if (strcmp("--log_controls", flag) == 0) {
            log_controls();
            return false;
        } else {
            vk_log(VK_ERROR, "unrecognized command");
            return false;
        }
    }

    return true;
}

void log_point_count(Point_Cloud_Layer *pc_layers, size_t lod)
{
    size_t point_count = 0;
    for (size_t layer = 0; layer <= lod; layer++) point_count += pc_layers[layer].count;
    vk_log(VK_INFO, "point count: %zu points, highest lod: %zu", point_count, lod);
}
