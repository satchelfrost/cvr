#include "cvr.h"

#define VK_CTX_IMPLEMENTATION
#include "vk_ctx.h"

#define RAYMATH_IMPLEMENTATION
#include "ext/raylib-5.0/raymath.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <time.h>     // used by nanosleep
#include "geometry.h" // defines some default primitives

#define MAX_KEYBOARD_KEYS 512
#define MAX_KEY_PRESSED_QUEUE 16
#define MAX_CHAR_PRESSED_QUEUE 16
#define CAMERA_MOVE_SPEED 10.0f
#define CAMERA_MOUSE_MOVE_SENSITIVITY 0.001f
#define CAMERA_ROT_SENSITIVITY 0.1f
#define GAMEPAD_ROT_SENSITIVITY 1.0f
#define MAX_MOUSE_BUTTONS 8
#define MAX_GAMEPAD_BUTTONS 32
#define MAX_GAMEPAD_AXIS 8
#define FPS_CAPTURE_FRAMES_COUNT 30
#define FPS_AVERAGE_TIME_SECONDS 0.5f
#define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)
#define DEAD_ZONE 0.25f

typedef struct {
    int exit_key;
    char curr_key_state[MAX_KEYBOARD_KEYS];
    char prev_key_state[MAX_KEYBOARD_KEYS];
    char key_repeat_in_frame[MAX_KEYBOARD_KEYS];
    int key_pressed_queue[MAX_KEY_PRESSED_QUEUE];
    int key_pressed_queue_count;
    int char_pressed_queue[MAX_CHAR_PRESSED_QUEUE];
    int char_pressed_queue_count;
} Keyboard;

typedef struct {
    Vector2 prev_pos;
    Vector2 curr_pos;
    Vector2 curr_wheel_move;
    Vector2 prev_wheel_move;
    char curr_button_state[MAX_MOUSE_BUTTONS];
    char prev_button_state[MAX_MOUSE_BUTTONS];
} Mouse;

typedef struct {
    float axis_state[MAX_GAMEPAD_AXIS];
    char curr_button_state[MAX_GAMEPAD_BUTTONS];
    char prev_button_state[MAX_GAMEPAD_BUTTONS];
    int last_button_pressed;
} Gamepad;

typedef struct {
    double curr;
    double prev;
    double update;
    double draw;
    double frame;
    double target;
    size_t frame_count;
} Time;

typedef struct {
    Vk_Buffer vtx_buff;
    Vk_Buffer idx_buff;
    const Vertex *verts;
    const uint16_t *idxs;
} Shape;

typedef struct {
    Vk_Buffer *items;
    size_t count;
    size_t capacity;
} Point_Clouds;

typedef struct {
    Matrix view;
    Matrix proj;
    Matrix view_proj;
} Matrices;

typedef enum {
    DEFAULT_PL_FILL,
    DEFAULT_PL_WIREFRAME,
    DEFAULT_PL_COUNT,
} Default_Pipeline;

typedef struct {
    VkPipeline handles[DEFAULT_PL_COUNT];
    VkPipelineLayout layouts[DEFAULT_PL_COUNT];
} Default_Pipelines;

/* core state global to all platforms */
/* TODO: put the core inside of a struct */
#define MAX_MAT_STACK 1024 * 1024
Matrix mat_stack[MAX_MAT_STACK];
size_t mat_stack_p = 0;
Shape shapes[SHAPE_COUNT];

#ifndef PLATFORM_QUEST
    Default_Pipelines pipelines = {0};
    Matrices matrices = {0};
    Point_Clouds point_clouds = {0};
    Keyboard keyboard = {0};
    Mouse mouse = {0};
    Gamepad gamepad = {0};
    Time cvr_time = {0};
    Window_Size win_size = {0};
    bool full_screen = false;
#else // clang I hate you
    Default_Pipelines pipelines = {};
    Matrices matrices = {};
    Point_Clouds point_clouds = {};
    Keyboard keyboard = {};
    Mouse mouse = {};
    Gamepad gamepad = {};
    Time cvr_time = {};
    Window_Size win_size = {};
    bool full_screen = false;
#endif
const char *core_title;

bool alloc_shape_res(Shape_Type shape_type);
bool is_shape_res_alloc(Shape_Type shape_type);
void destroy_shape_res();

#if defined(PLATFORM_DESKTOP_GLFW)
    #include "platform_desktop.c"
#elif defined(PLATFORM_ANDROID_QUEST)
    #include "platform_quest.c"
#else
    /* alternative backend here */
#endif

bool init_window(int width, int height, const char *title)
{
    win_size.width = width;
    win_size.height = height;
    core_title = title;

    if (!init_platform()) return false;

    /* initialize vulkan */
    if (!vk_init()) return false;
    else return true;
}

void close_window()
{
    vkDeviceWaitIdle(vk_ctx.device);

    for (size_t i = 0; i < DEFAULT_PL_COUNT; i++)
        vk_destroy_pl_res(pipelines.handles[i], pipelines.layouts[i]);

    destroy_shape_res();
    vk_destroy();
    close_platform();
}

void enable_full_screen()
{
    full_screen = true;
}

bool default_pl_fill_init()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(float16),
    };
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!vk_pl_layout_init(layout_ci, &pipelines.layouts[DEFAULT_PL_FILL])) return false;

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
        .pl_layout = pipelines.layouts[DEFAULT_PL_FILL],
        .vert = "./res/default.vert.glsl.spv",
        .frag = "./res/default.frag.glsl.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = VK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!vk_basic_pl_init(config, &pipelines.handles[DEFAULT_PL_FILL])) return false;

    return true;
}

bool draw_shape(Shape_Type shape_type)
{
    /* create basic shape pipeline if it hasn't been created */
    if (!pipelines.handles[DEFAULT_PL_FILL])
        if (!default_pl_fill_init()) return false;

    if (!is_shape_res_alloc(shape_type)) alloc_shape_res(shape_type);

    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        vk_log(VK_ERROR, "No matrix stack, cannot draw.");
        return false;
    }

    Vk_Buffer vtx_buff = shapes[shape_type].vtx_buff;
    Vk_Buffer idx_buff = shapes[shape_type].idx_buff;
    Matrix mvp = MatrixMultiply(model, matrices.view_proj);
    float16 f16_mvp = MatrixToFloatV(mvp);

    vk_bind_gfx(pipelines.handles[DEFAULT_PL_FILL], pipelines.layouts[DEFAULT_PL_FILL], NULL, 0);
    vk_push_const(pipelines.layouts[DEFAULT_PL_FILL], VK_SHADER_STAGE_VERTEX_BIT, sizeof(float16), &f16_mvp);
    vk_draw_buffers(vtx_buff, idx_buff);

    return true;
}

bool draw_shape_ex(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds, Shape_Type shape)
{
    if (!is_shape_res_alloc(shape)) alloc_shape_res(shape);

    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        vk_log(VK_ERROR, "No matrix stack, cannot draw.");
        return false;
    }

    Matrix mvp = MatrixMultiply(model, matrices.view_proj);
    float16 f16_mvp = MatrixToFloatV(mvp);
    Vk_Buffer vtx_buff = shapes[shape].vtx_buff;
    Vk_Buffer idx_buff = shapes[shape].idx_buff;

    vk_bind_gfx(pl, pl_layout, &ds, 1);
    vk_push_const(pl_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float16), &f16_mvp);
    vk_draw_buffers(vtx_buff, idx_buff);

    return true;
}

bool default_pl_wireframe_init()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(float16),
    };
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!vk_pl_layout_init(layout_ci, &pipelines.layouts[DEFAULT_PL_WIREFRAME]))
        return false;

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
        .pl_layout = pipelines.layouts[DEFAULT_PL_WIREFRAME],
        .vert = "./res/default.vert.glsl.spv",
        .frag = "./res/default.frag.glsl.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_LINE,
        .vert_attrs = vert_attrs,
        .vert_attr_count = VK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!vk_basic_pl_init(config, &pipelines.handles[DEFAULT_PL_WIREFRAME])) return false;

    return true;
}

bool draw_shape_wireframe(Shape_Type shape_type)
{
    /* create basic wireframe pipeline if it hasn't been created */
    if (!pipelines.handles[DEFAULT_PL_WIREFRAME])
        if (!default_pl_wireframe_init()) return false;

    if (!is_shape_res_alloc(shape_type)) alloc_shape_res(shape_type);

    Vk_Buffer vtx_buff = shapes[shape_type].vtx_buff;
    Vk_Buffer idx_buff = shapes[shape_type].idx_buff;
    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        vk_log(VK_ERROR, "No matrix stack, cannot draw.");
        return false;
    }

    Matrix mvp = MatrixMultiply(model, matrices.view_proj);
    float16 f16_mvp = MatrixToFloatV(mvp);

    vk_bind_gfx(pipelines.handles[DEFAULT_PL_WIREFRAME], pipelines.layouts[DEFAULT_PL_WIREFRAME], NULL, 0);
    vk_push_const(pipelines.layouts[DEFAULT_PL_WIREFRAME], VK_SHADER_STAGE_VERTEX_BIT, sizeof(float16), &f16_mvp);
    vk_draw_buffers(vtx_buff, idx_buff);

    return true;
}

Matrix get_proj(Camera camera)
{
    Matrix proj = {0};
    double aspect = vk_ctx.extent.width / (double) vk_ctx.extent.height;
    double top = camera.fovy / 2.0;
    double right = top * aspect;
    switch (camera.projection) {
    case PERSPECTIVE:
        proj  = MatrixPerspective(camera.fovy * DEG2RAD, aspect, Z_NEAR, Z_FAR);
        break;
    case ORTHOGRAPHIC:
        proj  = MatrixOrtho(-right, right, -top, top, -Z_FAR, Z_FAR);
        break;
    default:
        assert(0 && "unrecognized camera mode");
        break;
    }

    /* Vulkan */
    proj.m5 *= -1.0f;

    return proj;
}

Matrix get_proj_aspect(Camera camera, double aspect)
{
    Matrix proj = {0};
    double top = camera.fovy / 2.0;
    double right = top * aspect;
    switch (camera.projection) {
    case PERSPECTIVE:
        proj  = MatrixPerspective(camera.fovy * DEG2RAD, aspect, Z_NEAR, Z_FAR);
        break;
    case ORTHOGRAPHIC:
        proj  = MatrixOrtho(-right, right, -top, top, -Z_FAR, Z_FAR);
        break;
    default:
        assert(0 && "unrecognized camera mode");
        break;
    }

    /* Vulkan */
    proj.m5 *= -1.0f;

    return proj;
}

void begin_mode_3d(Camera camera)
{
    matrices.proj = get_proj(camera);
    matrices.view = MatrixLookAt(camera.position, camera.target, camera.up);
    matrices.view_proj = MatrixMultiply(matrices.view, matrices.proj);

    push_matrix();
}

Matrix get_view_proj()
{
    return matrices.view_proj;
}

bool get_mvp(Matrix *mvp)
{
    bool result = true;

    Matrix model = {0};
    if (!get_matrix_tos(&model)) vk_return_defer(false);
    *mvp = MatrixMultiply(model, matrices.view_proj);

defer:
    return result;
}

bool get_mvp_float16(float16 *mvp)
{
    bool result = true;

    Matrix m = {0};
    if (!get_mvp(&m)) vk_return_defer(false);
    *mvp = MatrixToFloatV(m);

defer:
    return result;
}

static void wait_time(double seconds)
{
    if (seconds <= 0) return;

    /* prepare for partial busy wait loop */
    double destination_time = get_time() + seconds;
    double sleep_secs = seconds - seconds * 0.05;

    /* for now wait time only supports linux */
#if defined(__linux__)
    struct timespec req = {0};
    time_t sec = sleep_secs;
    long nsec = (sleep_secs - sec) * 1000000000L;
    req.tv_sec = sec;
    req.tv_nsec = nsec;

    while (nanosleep(&req, &req) == -1) continue;
#endif

#if defined(_WIN32)
    Sleep((unsigned long)(sleep_secs * 1000.0));
#endif

    /* partial busy wait loop */
    while (get_time() < destination_time) {}
}

void begin_timer()
{
    cvr_time.curr   = get_time();
    cvr_time.update = cvr_time.curr - cvr_time.prev;
    cvr_time.prev   = cvr_time.curr;
}

void end_timer()
{
    cvr_time.curr = get_time();
    cvr_time.draw = cvr_time.curr - cvr_time.prev;
    cvr_time.prev = cvr_time.curr;
    cvr_time.frame = cvr_time.update + cvr_time.draw;

    if (cvr_time.frame < cvr_time.target) {
        wait_time(cvr_time.target - cvr_time.frame);

        cvr_time.curr = get_time();
        double wait = cvr_time.curr - cvr_time.prev;
        cvr_time.prev = cvr_time.curr;
        cvr_time.frame += wait;
    }

    cvr_time.frame_count++;
}

void begin_drawing(Color color)
{
    begin_timer();
    vk_wait_to_begin_gfx();
    vk_begin_rec_gfx();
    vk_begin_render_pass(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
}

void end_drawing()
{
    vk_end_render_pass();
    vk_end_rec_gfx();
    vk_submit_gfx();

    end_timer();
    poll_input_events();
}

void push_matrix()
{
    if (mat_stack_p < MAX_MAT_STACK) {
        if (mat_stack_p) {
            mat_stack[mat_stack_p] = mat_stack[mat_stack_p - 1];
            mat_stack_p++;
        } else {
            mat_stack[mat_stack_p++] = MatrixIdentity();
        }
    } else {
        vk_log(VK_ERROR, "matrix stack overflow");
    }
}

void pop_matrix()
{
    if (mat_stack_p > 0)
        mat_stack_p--;
    else
        vk_log(VK_ERROR, "matrix stack underflow");
}

void end_mode_3d()
{
    pop_matrix();

    size_t leftover = 0;
    while(mat_stack_p > 0) {
        pop_matrix();
        leftover++;
    }

    if (leftover)
        vk_log(VK_WARNING, "%d matrix stack(s) leftover", leftover);
}

bool is_key_pressed(int key)
{
    bool pressed = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS)) {
        if ((keyboard.prev_key_state[key] == 0) && (keyboard.curr_key_state[key] == 1))
            pressed = true;
    }

    return pressed;
}

bool is_key_down(int key)
{
    bool down = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS)) {
        if (keyboard.curr_key_state[key] == 1) down = true;
    }

    return down;
}

bool is_gamepad_button_pressed(int button)
{
    bool pressed = false;

    if (button < MAX_GAMEPAD_BUTTONS) {
        if (gamepad.prev_button_state[button] == 0 && gamepad.curr_button_state[button] == 1)
            pressed = true;
    }

    return pressed;
}

bool is_gamepad_button_down(int button)
{
    bool pressed = false;

    if (button < MAX_GAMEPAD_BUTTONS) {
        if (gamepad.curr_button_state[button] == 1)
            pressed = true;
    }

    return pressed;
}

float get_gamepad_axis_movement(int axis)
{
    float value = 0;

    if (axis < MAX_GAMEPAD_AXIS && fabsf(gamepad.axis_state[axis]) > 0.1f)
        value = gamepad.axis_state[axis];

    return value;
}

int get_last_btn_pressed()
{
    return gamepad.last_button_pressed;
}

void add_matrix(Matrix matrix)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(matrix, mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available on current stack");
}

void translate(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixTranslate(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available to translate");
}

void rotate(Vector3 axis, float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotate(axis, angle), mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available to rotate");
}

void rotate_x(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateX(angle), mat_stack[mat_stack_p - 1]);
    else
    vk_log(VK_ERROR, "no matrix available to rotate x");
}

void rotate_y(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateY(angle), mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available to rotate y");
}

void rotate_z(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateZ(angle), mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available to rotate z");
}

void rotate_xyz(Vector3 angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateXYZ(angle), mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available to rotate xyz");
}

void rotate_zyx(Vector3 angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateZYX(angle), mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available to rotate zyx");
}

void scale(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixScale(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        vk_log(VK_ERROR, "no matrix available to scale");
}

bool alloc_shape_res(Shape_Type shape_type)
{
    bool result = true;

    if (is_shape_res_alloc(shape_type)) {
        vk_log(VK_WARNING, "Shape resources for shape %d have already been allocated", shape_type);
        vk_return_defer(false);
    }

    Shape *shape = &shapes[shape_type];
    switch (shape_type) {
#define X(CONST_NAME, array_name)                                                     \
    case SHAPE_ ## CONST_NAME:                                                        \
    shape->vtx_buff.count = CONST_NAME ## _VERTS;                                     \
    shape->vtx_buff.size = sizeof(array_name ## _verts[0]) * shape->vtx_buff.count;   \
    shape->verts = array_name ## _verts;                                              \
    shape->idx_buff.count = CONST_NAME ## _IDXS;                                      \
    shape->idx_buff.size = sizeof(array_name ## _indices[0]) * shape->idx_buff.count; \
    shape->idxs = array_name ## _indices;                                             \
    break;
    SHAPE_LIST
#undef X
    default:
        vk_log(VK_ERROR, "unrecognized shape %d", shape_type);
        vk_return_defer(false);
    }

    if ((!vk_vtx_buff_upload(&shape->vtx_buff, shape->verts)) || (!vk_idx_buff_upload(&shape->idx_buff, shape->idxs))) {
        vk_log(VK_ERROR, "failed to allocate resources for shape %d", shape_type);
        vk_return_defer(false);
    }

defer:
    return result;
}

bool is_shape_res_alloc(Shape_Type shape_type)
{
    assert((shape_type >= 0 && shape_type < SHAPE_COUNT) && "invalid shape");
    return (shapes[shape_type].vtx_buff.handle || shapes[shape_type].idx_buff.handle);
}

void destroy_shape_res()
{
    for (size_t i = 0; i < SHAPE_COUNT; i++) {
        if (shapes[i].vtx_buff.handle)
            vk_buff_destroy(&shapes[i].vtx_buff);
        if (shapes[i].idx_buff.handle)
            vk_buff_destroy(&shapes[i].idx_buff);
    }
}

Vector3 get_camera_forward(Camera *camera)
{
    return Vector3Normalize(Vector3Subtract(camera->target, camera->position));
}

Vector3 get_camera_up(Camera *camera)
{
    return Vector3Normalize(camera->up);
}

Vector3 get_camera_right(Camera *camera)
{
    Vector3 forward = get_camera_forward(camera);
    Vector3 up = get_camera_up(camera);
    return Vector3CrossProduct(forward, up);
}

void camera_move_forward(Camera *camera, float distance)
{
    Vector3 forward = get_camera_forward(camera);
    forward = Vector3Scale(forward, distance);
    camera->position = Vector3Add(camera->position, forward);
    camera->target = Vector3Add(camera->target, forward);
}

void camera_move_right(Camera *camera, float distance)
{
    Vector3 right = get_camera_right(camera);
    right = Vector3Scale(right, distance);
    camera->position = Vector3Add(camera->position, right);
    camera->target = Vector3Add(camera->target, right);
}

void camera_move_up(Camera *camera, float distance)
{
    Vector3 up = get_camera_up(camera);
    up = Vector3Scale(up, distance);
    camera->position = Vector3Add(camera->position, up);
    camera->target = Vector3Add(camera->target, up);
}

Vector2 get_mouse_delta()
{
    Vector2 delta = {
        .x = mouse.curr_pos.x - mouse.prev_pos.x,
        .y = mouse.curr_pos.y - mouse.prev_pos.y
    };
    return delta;
}

void camera_yaw(Camera *camera, float angle)
{
    Vector3 up = get_camera_up(camera);
    Vector3 target_pos = Vector3Subtract(camera->target, camera->position);
    target_pos = Vector3RotateByAxisAngle(target_pos, up, angle);
    camera->target = Vector3Add(camera->position, target_pos);
}

void camera_roll(Camera *camera, float angle)
{
    Vector3 forward = get_camera_forward(camera);
    camera->up = Vector3RotateByAxisAngle(camera->up, forward, angle);
}

void camera_pitch(Camera *camera, float angle)
{
    Vector3 right = get_camera_right(camera);
    Vector3 target_pos = Vector3Subtract(camera->target, camera->position);
    target_pos = Vector3RotateByAxisAngle(target_pos, right, angle);
    camera->target = Vector3Add(camera->position, target_pos);
}

void camera_move_to_target(Camera *camera, float delta)
{
    float distance = Vector3Distance(camera->position, camera->target);
    distance += delta;
    if (distance <= 0) distance = 0.001f;
    Vector3 forward = get_camera_forward(camera);
    camera->position = Vector3Add(camera->target, Vector3Scale(forward, -distance));
}

void update_camera_free(Camera *camera)
{
    Vector2 delta = get_mouse_delta();

    if (is_mouse_button_down(MOUSE_BUTTON_RIGHT)) {
        camera_yaw(camera, -delta.x * CAMERA_MOUSE_MOVE_SENSITIVITY);
        camera_pitch(camera, -delta.y * CAMERA_MOUSE_MOVE_SENSITIVITY);
    }

    float ft = get_frame_time();
    float move_speed = CAMERA_MOVE_SPEED * ft;

    /* gamepad movement */
    float joy_x = get_gamepad_axis_movement(GAMEPAD_AXIS_RIGHT_X);
    float joy_y = get_gamepad_axis_movement(GAMEPAD_AXIS_RIGHT_Y);
    float joy_x_norm = (fabsf(joy_x) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    float joy_y_norm = (fabsf(joy_y) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    if (joy_x >  DEAD_ZONE) camera_yaw(camera,  -joy_x_norm * ft * GAMEPAD_ROT_SENSITIVITY);
    if (joy_y >  DEAD_ZONE) camera_pitch(camera,-joy_y_norm * ft * GAMEPAD_ROT_SENSITIVITY);
    if (joy_x < -DEAD_ZONE) camera_yaw(camera,   joy_x_norm * ft * GAMEPAD_ROT_SENSITIVITY);
    if (joy_y < -DEAD_ZONE) camera_pitch(camera, joy_y_norm * ft * GAMEPAD_ROT_SENSITIVITY);
    float fb = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_Y);
    float lr = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_X);
    float fb_norm = (fabsf(fb) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    float lr_norm = (fabsf(lr) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    if (fb <= -DEAD_ZONE) camera_move_forward(camera,  move_speed * fb_norm);
    if (lr <= -DEAD_ZONE) camera_move_right(camera,   -move_speed * lr_norm);
    if (fb >=  DEAD_ZONE) camera_move_forward(camera, -move_speed * fb_norm);
    if (lr >=  DEAD_ZONE) camera_move_right(camera,    move_speed * lr_norm);
    if (is_gamepad_button_down(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) camera_move_up(camera, move_speed / 2.0f);
    if (is_gamepad_button_down(GAMEPAD_BUTTON_LEFT_TRIGGER_2))  camera_move_up(camera, -move_speed / 2.0f);

    /* keyboard movement */
    if (is_key_down(KEY_LEFT_SHIFT)) move_speed *= 10.0f;
    if (is_key_down(KEY_W)) camera_move_forward(camera,  move_speed);
    if (is_key_down(KEY_A)) camera_move_right(camera,   -move_speed);
    if (is_key_down(KEY_S)) camera_move_forward(camera, -move_speed);
    if (is_key_down(KEY_D)) camera_move_right(camera,    move_speed);
    if (is_key_down(KEY_E)) camera_move_up(camera,  move_speed);
    if (is_key_down(KEY_Q)) camera_move_up(camera, -move_speed);
    if (is_key_down(KEY_LEFT))  camera_roll(camera, -CAMERA_ROT_SENSITIVITY * ft);
    if (is_key_down(KEY_RIGHT)) camera_roll(camera,  CAMERA_ROT_SENSITIVITY * ft);

    camera_move_to_target(camera, -get_mouse_wheel_move());
}

int get_mouse_x()
{
    return (int)mouse.curr_pos.x;
}

int get_mouse_y()
{
    return (int)mouse.curr_pos.y;
}

bool is_mouse_button_down(int button)
{
    return mouse.curr_button_state[button] == 1;
}

bool draw_points(Vk_Buffer vtx_buff, VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet *ds_sets, size_t ds_set_count)
{
    if (!vtx_buff.handle) {
        vk_log(VK_ERROR, "vertex buffer was not uploaded for point cloud");
        return false;
    }

    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        vk_log(VK_ERROR, "No matrix stack, cannot draw.");
        return false;
    }
    Matrix mvp = MatrixMultiply(model, matrices.view_proj);
    float16 f16_mvp = MatrixToFloatV(mvp);
    vk_draw_points(vtx_buff, &f16_mvp, pl, pl_layout, ds_sets, ds_set_count);

    return true;
}

double get_frame_time()
{
    return cvr_time.frame;	
}

int get_average_fps()
{
    int fps = 0;

    static int index = 0;
    static double history[FPS_CAPTURE_FRAMES_COUNT] = {0};
    static double average = 0, last = 0;
    double fps_frame = get_frame_time();

    if (cvr_time.frame_count == 0) {
        average = 0;
        last = 0;
        index = 0;

        for (int i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fps_frame == 0) return 0;

    if ((get_time() - last) > FPS_STEP)
    {
        last = get_time();
        index = (index + 1) % FPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = fps_frame / FPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    fps = (int)roundf((float)1.0f/average);

    return fps;
}

int get_fps() // TODO: another function that looks at just ms
{
    double frame_time = get_frame_time();
    if (frame_time == 0) return 0;
    else return (int)roundf(1.0f / frame_time);
}

void set_target_fps(int fps)
{
    if (fps < 1) cvr_time.target = 0.0;
    else cvr_time.target = 1.0 / (double) fps;
    vk_log(VK_INFO, "target fps: %02.03f ms", (float) cvr_time.target * 1000.0f);
}

void look_at(Camera camera)
{
    /* Note we are using MatrixInvert here because matrix look at actually
     * takes the inverse because it assumes it will be used as a view matrix.
     * In this case we actually want the world matrix */
    Matrix inv = MatrixInvert(MatrixLookAt(camera.position, camera.target, camera.up));
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(mat_stack[mat_stack_p - 1], inv);
    else
        vk_log(VK_ERROR, "no matrix available to translate");
}

bool get_matrix_tos(Matrix *model)
{
    if (mat_stack_p) {
        *model = mat_stack[mat_stack_p - 1];
    } else {
        vk_log(VK_ERROR, "No matrix on stack");
        return false;
    }

    return true;
}

Color color_from_HSV(float hue, float saturation, float value)
{
    Color color = { 0, 0, 0, 255 };

    // Red channel
    float k = fmodf((5.0f + hue/60.0f), 6);
    float t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.r = (unsigned char)((value - value*saturation*k)*255.0f);

    // Green channel
    k = fmodf((3.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.g = (unsigned char)((value - value*saturation*k)*255.0f);

    // Blue channel
    k = fmodf((1.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.b = (unsigned char)((value - value*saturation*k)*255.0f);

    return color;
}

Window_Size get_window_size()
{
    return win_size;
}

void wait_idle() // TODO: this should be in vk_ctx.h
{
    vkDeviceWaitIdle(vk_ctx.device);
}

void log_fps()
{
    static int fps = -1;
    int curr_fps = get_fps();
    if (curr_fps != fps) {
        vk_log(VK_INFO, "FPS %d (%.3f)", curr_fps, get_frame_time());
        fps = curr_fps;
    }
}

float get_mouse_wheel_move()
{
    float result = 0.0f;

    if (fabsf(mouse.curr_wheel_move.x) > fabsf(mouse.curr_wheel_move.y)) result = (float)mouse.curr_wheel_move.x;
    else result = (float)mouse.curr_wheel_move.y;

    return result;
}
