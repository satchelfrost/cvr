#include "cvr.h"
#include <stdbool.h>
#include <vulkan/vulkan_core.h>

#define RAYMATH_IMPLEMENTATION
#include "ext/raylib-5.0/raymath.h"

#define VK_CTX_IMPLEMENTATION
#include "vk_ctx.h"

#define NOB_IMPLEMENTATION
#include "ext/nob.h"

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

#define MAX_KEYBOARD_KEYS 512
#define MAX_KEY_PRESSED_QUEUE 16
#define MAX_CHAR_PRESSED_QUEUE 16
#define CAMERA_MOVE_SPEED 10.0f
#define CAMERA_MOUSE_MOVE_SENSITIVITY 0.001f
#define CAMERA_ROTATION_SPEED 1.0f
#define MAX_MOUSE_BUTTONS 8
#define FPS_CAPTURE_FRAMES_COUNT 30
#define FPS_AVERAGE_TIME_SECONDS 0.5f
#define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)

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
    Matrix viewProj;
} Matrices;

/* State */
Matrices matrices = {0};
Point_Clouds point_clouds = {0};
Keyboard keyboard = {0};
Mouse mouse = {0};
Time cvr_time = {0};
#define MAX_MAT_STACK 1024 * 1024
Matrix mat_stack[MAX_MAT_STACK];
size_t mat_stack_p = 0;
Shape shapes[SHAPE_COUNT];

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset);
void poll_input_events();
bool alloc_shape_res(Shape_Type shape_type);
bool is_shape_res_alloc(Shape_Type shape_type);
float get_mouse_wheel_move();
void destroy_ubos();

bool init_window(int width, int height, const char *title)
{
    bool result = true;

    /* Initialize glfw stuff */
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ctx.window = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwSetWindowUserPointer(ctx.window, &ctx);
    glfwSetFramebufferSizeCallback(ctx.window, frame_buff_resized);
    glfwSetKeyCallback(ctx.window, key_callback);
    glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
    glfwSetCursorPosCallback(ctx.window, mouse_cursor_pos_callback);
    glfwSetScrollCallback(ctx.window, mouse_scroll_callback);

    cvr_chk(vk_init(), "failed to initialize Vulkan context");

defer:
    return result;
}

bool window_should_close()
{
    bool result = glfwWindowShouldClose(ctx.window);
    glfwPollEvents();
    return result;
}

bool draw_shape(Shape_Type shape_type)
{
    bool result = true;

    if (!ctx.pipelines[PIPELINE_DEFAULT])
        if (!vk_basic_pl_init(PIPELINE_DEFAULT))
            nob_return_defer(false);

    if (!is_shape_res_alloc(shape_type)) alloc_shape_res(shape_type);

    Vk_Buffer vtx_buff = shapes[shape_type].vtx_buff;
    Vk_Buffer idx_buff = shapes[shape_type].idx_buff;
    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        nob_log(NOB_ERROR, "No matrix stack, cannot draw.");
        nob_return_defer(false);
    }

    Matrix mvp = MatrixMultiply(model, matrices.viewProj);
    result = vk_draw(PIPELINE_DEFAULT, vtx_buff, idx_buff, mvp);

defer:
    return result;
}

bool draw_shape_wireframe(Shape_Type shape_type)
{
    bool result = true;

    if (!ctx.pipelines[PIPELINE_WIREFRAME])
        if (!vk_basic_pl_init(PIPELINE_WIREFRAME))
            nob_return_defer(false);

    if (!is_shape_res_alloc(shape_type)) alloc_shape_res(shape_type);

    Vk_Buffer vtx_buff = shapes[shape_type].vtx_buff;
    Vk_Buffer idx_buff = shapes[shape_type].idx_buff;
    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        nob_log(NOB_ERROR, "No matrix stack, cannot draw.");
        nob_return_defer(false);
    }

    Matrix mvp = MatrixMultiply(model, matrices.viewProj);
    result = vk_draw(PIPELINE_WIREFRAME, vtx_buff, idx_buff, mvp);

defer:
    return result;
}

Matrix get_proj(Camera camera)
{
    Matrix proj = {0};
    double aspect = ctx.extent.width / (double) ctx.extent.height;
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
    matrices.viewProj = MatrixMultiply(matrices.view, matrices.proj);

    push_matrix();
}

static void wait_time(double seconds)
{
    if (seconds <= 0) return;

    /* prepare for partial busy wait loop */
    double destination_time = get_time() + seconds;
    double sleep_secs = seconds - seconds * 0.05;

    /* for now wait time only supports linux */ // TODO: Windows
    struct timespec req = {0};
    time_t sec = sleep_secs;
    long nsec = (sleep_secs - sec) * 1000000000L;
    req.tv_sec = sec;
    req.tv_nsec = nsec;

    while (nanosleep(&req, &req) == -1) continue;

    /* partial busy wait loop */
    while (get_time() < destination_time) {}
}

void begin_drawing(Color color)
{
    cvr_time.curr   = get_time();
    cvr_time.update = cvr_time.curr - cvr_time.prev;
    cvr_time.prev   = cvr_time.curr;

    vk_begin_drawing();
    vk_begin_render_pass(color);
}

void end_drawing()
{
    for (size_t i = 0; i < UBO_TYPE_COUNT; i++) {
        UBO ubo = ctx.ubos[i];
        if (ubo.buff.handle)
            memcpy(ubo.buff.mapped, ubo.data, ubo.buff.size);
    }

    vk_end_drawing();

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

    poll_input_events();
    cvr_time.frame_count++;
}

void begin_compute()
{
    assert(vk_begin_compute() && "failed to begin compute");
}

void end_compute()
{
    assert(vk_end_compute() && "failed to end compute");
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
        nob_log(NOB_ERROR, "matrix stack overflow");
    }
}

void pop_matrix()
{
    if (mat_stack_p > 0)
        mat_stack_p--;
    else
        nob_log(NOB_ERROR, "matrix stack underflow");
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
        nob_log(NOB_WARNING, "%d matrix stack(s) leftover", leftover);
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

void poll_input_events()
{
    keyboard.key_pressed_queue_count = 0;
    keyboard.char_pressed_queue_count = 0;

    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) {
        keyboard.prev_key_state[i] = keyboard.curr_key_state[i];
        keyboard.key_repeat_in_frame[i] = 0;
    }

    mouse.prev_pos = mouse.curr_pos;
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
        mouse.prev_button_state[i] = mouse.curr_button_state[i];

    mouse.prev_wheel_move = mouse.curr_wheel_move;
    mouse.curr_wheel_move = (Vector2){ 0.0f, 0.0f };
}

float get_mouse_wheel_move()
{
    float result = 0.0f;

    if (fabsf(mouse.curr_wheel_move.x) > fabsf(mouse.curr_wheel_move.y)) result = (float)mouse.curr_wheel_move.x;
    else result = (float)mouse.curr_wheel_move.y;

    return result;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    if (key < 0) return;

    switch (action) {
    case GLFW_RELEASE: keyboard.curr_key_state[key] = 0; break;
    case GLFW_PRESS: keyboard.curr_key_state[key] = 1; break;
    case GLFW_REPEAT: keyboard.key_repeat_in_frame[key] = 1; break;
    }

    if (((key == KEY_CAPS_LOCK) && ((mods & GLFW_MOD_CAPS_LOCK) > 0)) ||
        ((key == KEY_NUM_LOCK) && ((mods & GLFW_MOD_NUM_LOCK) > 0))) keyboard.curr_key_state[key] = 1;

    if ((keyboard.key_pressed_queue_count < MAX_KEY_PRESSED_QUEUE) && (action == GLFW_PRESS)) {
        keyboard.key_pressed_queue[keyboard.key_pressed_queue_count] = key;
        keyboard.key_pressed_queue_count++;
    }

    if ((key == keyboard.exit_key) && (action == GLFW_PRESS)) glfwSetWindowShouldClose(window, GLFW_TRUE);
}

double get_time()
{
    return glfwGetTime();
}

void translate(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixTranslate(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        nob_log(NOB_ERROR, "no matrix available to translate");
}

void rotate(Vector3 axis, float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotate(axis, angle), mat_stack[mat_stack_p - 1]);
    else
        nob_log(NOB_ERROR, "no matrix available to rotate");
}

void rotate_x(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateX(angle), mat_stack[mat_stack_p - 1]);
    else
    nob_log(NOB_ERROR, "no matrix available to rotate x");
}

void rotate_y(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateY(angle), mat_stack[mat_stack_p - 1]);
    else
        nob_log(NOB_ERROR, "no matrix available to rotate y");
}

void rotate_z(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateZ(angle), mat_stack[mat_stack_p - 1]);
    else
        nob_log(NOB_ERROR, "no matrix available to rotate z");
}

void rotate_xyz(Vector3 angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateXYZ(angle), mat_stack[mat_stack_p - 1]);
    else
        nob_log(NOB_ERROR, "no matrix available to rotate xyz");
}

void rotate_zyx(Vector3 angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateZYX(angle), mat_stack[mat_stack_p - 1]);
    else
        nob_log(NOB_ERROR, "no matrix available to rotate zyx");
}

void scale(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixScale(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        nob_log(NOB_ERROR, "no matrix available to scale");
}

bool alloc_shape_res(Shape_Type shape_type)
{
    bool result = true;

    if (is_shape_res_alloc(shape_type)) {
        nob_log(NOB_WARNING, "Shape resources for shape %d have already been allocated", shape_type);
        nob_return_defer(false);
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
        nob_log(NOB_ERROR, "unrecognized shape %d", shape_type);
        nob_return_defer(false);
    }

    if ((!vk_vtx_buff_upload(&shape->vtx_buff, shape->verts)) || (!vk_idx_buff_upload(&shape->idx_buff, shape->idxs))) {
        nob_log(NOB_ERROR, "failed to allocate resources for shape %d", shape_type);
        nob_return_defer(false);
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
            vk_buff_destroy(shapes[i].vtx_buff);
        if (shapes[i].idx_buff.handle)
            vk_buff_destroy(shapes[i].idx_buff);
    }
}

void close_window()
{
    vkDeviceWaitIdle(ctx.device);

    destroy_shape_res();
    destroy_ubos();
    vk_destroy();
    glfwDestroyWindow(ctx.window);
    glfwTerminate();
}

Image load_image(const char *file_name)
{
    Image img = {0};
    int channels;
    img.data = stbi_load(file_name, &img.width, &img.height, &channels, STBI_rgb_alpha);
    img.format = (int)VK_FORMAT_R8G8B8A8_SRGB;
    return img;
}

void unload_image(Image image)
{
    free(image.data);
}

Texture load_texture_from_image(Image img)
{
    Texture texture = {
        .width    = img.width,
        .height   = img.height,
        .mipmaps  = img.mipmaps,
        .format   = img.format,
    };

    if (!vk_load_texture(img.data, img.width, img.height, img.format, &texture.id, false))
        nob_log(NOB_ERROR, "unable to load texture");

    return texture;
}

Texture load_pc_texture_from_image(Image img)
{
    Texture texture = {
        .width    = img.width,
        .height   = img.height,
        .mipmaps  = img.mipmaps,
        .format   = img.format,
    };

    if (!vk_load_texture(img.data, img.width, img.height, img.format, &texture.id, true))
        nob_log(NOB_ERROR, "unable to load texture");

    return texture;
}

void unload_texture(Texture texture)
{
    vk_unload_texture(texture.id, SAMPLER_TYPE_ONE_TEX);
}

void unload_pc_texture(Texture texture)
{
    vk_unload_texture(texture.id, SAMPLER_TYPE_FOUR_TEX);
}

bool tex_sampler_init()
{
    Sampler_Type type = SAMPLER_TYPE_ONE_TEX;
    if (!vk_sampler_descriptor_set_layout_init(type))
        return false;
    if (!vk_sampler_descriptor_pool_init(type))
        return false;
    if (!vk_sampler_descriptor_set_init(type))
        return false;
    return true;
}

bool draw_texture(Texture texture, Shape_Type shape_type)
{
    bool result = true;

    if (!ctx.pipelines[PIPELINE_TEXTURE]) {
        if (!tex_sampler_init()) nob_return_defer(false);
        if (!vk_basic_pl_init(PIPELINE_TEXTURE))
            nob_return_defer(false);
    }

    if (!is_shape_res_alloc(shape_type)) alloc_shape_res(shape_type);

    Vk_Buffer vtx_buff = shapes[shape_type].vtx_buff;
    Vk_Buffer idx_buff = shapes[shape_type].idx_buff;

    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        nob_log(NOB_ERROR, "No matrix stack, cannot draw.");
        nob_return_defer(false);
    }

    Matrix mvp = MatrixMultiply(model, matrices.viewProj);
    if (!vk_draw_texture(texture.id, vtx_buff, idx_buff, mvp))
        nob_return_defer(false);

defer:
    return result;
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
    float rot_speed = ft * CAMERA_ROTATION_SPEED;

    if (is_key_down(KEY_K)) camera_pitch(camera, -rot_speed);
    if (is_key_down(KEY_I)) camera_pitch(camera,  rot_speed);
    if (is_key_down(KEY_L)) camera_yaw(camera,   -rot_speed);
    if (is_key_down(KEY_J)) camera_yaw(camera,    rot_speed);

    float move_speed = CAMERA_MOVE_SPEED * ft;
    if (is_key_down(KEY_LEFT_SHIFT))
        move_speed *= 10.0f;

    if (is_key_down(KEY_W)) camera_move_forward(camera,  move_speed);
    if (is_key_down(KEY_A)) camera_move_right(camera,   -move_speed);
    if (is_key_down(KEY_S)) camera_move_forward(camera, -move_speed);
    if (is_key_down(KEY_D)) camera_move_right(camera,    move_speed);
    if (is_key_down(KEY_E)) camera_move_up(camera,  move_speed);
    if (is_key_down(KEY_Q)) camera_move_up(camera, -move_speed);

    camera_move_to_target(camera, -get_mouse_wheel_move());
}

static void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y)
{
    (void) window;

    mouse.curr_pos.x = x;
    mouse.curr_pos.y = y;
}

int get_mouse_x()
{
    return (int)mouse.curr_pos.x;
}

int get_mouse_y()
{
    return (int)mouse.curr_pos.y;
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    (void) window;
    (void) mods;
    mouse.curr_button_state[button] = action;
}

bool is_mouse_button_down(int button)
{
    return mouse.curr_button_state[button] == GLFW_PRESS;
}

bool upload_point_cloud(Buffer buff, size_t *id)
{
    Vk_Buffer vk_buff = {
        .count = buff.count,
        .size  = buff.size,
    };
    if (!vk_vtx_buff_staged_upload(&vk_buff, buff.items)) {
        nob_log(NOB_ERROR, "failed to initialize vertex buffer for point cloud");
        return false;
    }

    *id = point_clouds.count;
    nob_da_append(&point_clouds, vk_buff);
    return true;
}

bool upload_compute_points(Buffer buff, size_t *id)
{
    Vk_Buffer vk_buff = {
        .count = buff.count,
        .size  = buff.size,
    };
    if (!vk_comp_buff_staged_upload(&vk_buff, buff.items)) {
        nob_log(NOB_ERROR, "failed to upload compute points");
        return false;
    }
    
    *id = ctx.compute_buffs.count;
    nob_da_append(&ctx.compute_buffs, vk_buff);

    return true;
}

bool ubo_init(Buffer buff, Example example)
{
    UBO_Type ubo_type; 
    VkShaderStageFlags flags;
    switch (example) {
    case EXAMPLE_TEX:
        ubo_type = UBO_TYPE_TEX;
        flags = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case EXAMPLE_ADV_POINT_CLOUD:
        ubo_type = UBO_TYPE_ADV_POINT_CLOUD;
        flags = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case EXAMPLE_COMPUTE:
        ubo_type = UBO_TYPE_COMPUTE;
        flags = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case EXAMPLE_POINT_CLOUD:
    default:
        nob_log(NOB_ERROR, "example %d not handled for ubo_init", example);
        return false;
    }

    UBO ubo = {
        .buff = {
            .count = buff.count,
            .size  = buff.size
        },
        .data = buff.items,
    };
    if (!vk_ubo_init(ubo, ubo_type)) return false;
    if (!vk_ubo_descriptor_set_layout_init(flags, ubo_type)) return false;
    if (!vk_ubo_descriptor_pool_init(ubo_type))              return false;
    if (!vk_ubo_descriptor_set_init(ubo_type))               return false;

    return true;
}

bool pc_sampler_init()
{
    Sampler_Type type = SAMPLER_TYPE_FOUR_TEX;
    if (!vk_sampler_descriptor_set_layout_init(type))
        return false;
    if (!vk_sampler_descriptor_pool_init(type))
        return false;
    if (!vk_sampler_descriptor_set_init(type))
        return false;

    return true;
}

void destroy_point_cloud(size_t id)
{
    vkDeviceWaitIdle(ctx.device);

    bool found = false;
    for (size_t i = 0; i < point_clouds.count; i++) {
        if (i == id && point_clouds.items[i].handle) {
            vk_buff_destroy(point_clouds.items[i]);
            found = true;
        }
    }

    if (!found)
        nob_log(NOB_WARNING, "point cloud &zu does not exist cannot destroy", id);
}

void destroy_ubos()
{
    vkDeviceWaitIdle(ctx.device);

    for (size_t i = 0; i < UBO_TYPE_COUNT; i++)
        if (ctx.ubos[i].buff.handle)
            vk_buff_destroy(ctx.ubos[i].buff);

    for (size_t i = 0; i < UBO_TYPE_COUNT; i++) {
        vkDestroyDescriptorPool(ctx.device, ctx.ubos[i].descriptor_pool, NULL);
        vkDestroyDescriptorSetLayout(ctx.device, ctx.ubos[i].set_layout, NULL);
    }
}

void destroy_compute_buff(size_t id)
{
    vkDeviceWaitIdle(ctx.device);

    bool found = false;
    for (size_t i = 0; i < ctx.compute_buffs.count; i++) {
        if (i == id && ctx.compute_buffs.items[i].handle) {
            vk_buff_destroy(ctx.compute_buffs.items[i]);
            found = true;
        }
    }

    if (!found)
        nob_log(NOB_WARNING, "compute buffer &zu does not exist cannot destroy", id);
}

bool compute_points()
{
    bool result = true;

    if (!ctx.compute_pipeline) {
        if (!vk_compute_pl_init())
            nob_return_defer(false);
    }

    vk_compute();

defer:
    return result;
}

bool draw_points(size_t vtx_id, Example example)
{
    bool result = true;

    if (example == EXAMPLE_ADV_POINT_CLOUD) {
        if (!ctx.pipelines[PIPELINE_POINT_CLOUD_ADV]) {
            pc_sampler_init();
            if (!vk_basic_pl_init(PIPELINE_POINT_CLOUD_ADV))
                nob_return_defer(false);
        }
    } else if (example == EXAMPLE_POINT_CLOUD) {
        if (!ctx.pipelines[PIPELINE_POINT_CLOUD])
            if (!vk_basic_pl_init(PIPELINE_POINT_CLOUD))
                nob_return_defer(false);
    } else if (example == EXAMPLE_COMPUTE) {
        if (!ctx.pipelines[PIPELINE_COMPUTE]) {
            if (!vk_basic_pl_init(PIPELINE_COMPUTE))
                nob_return_defer(false);
        }
    } else {
        nob_log(NOB_ERROR, "no other example supported yet for draw points");
        nob_return_defer(false);
    }

    Vk_Buffer vtx_buff = {0};
    if (example == EXAMPLE_COMPUTE) {
        if (vtx_id < ctx.compute_buffs.count && ctx.compute_buffs.items[vtx_id].handle) {
            vtx_buff = ctx.compute_buffs.items[vtx_id];
        } else {
            nob_log(NOB_ERROR, "vertex buffer was not uploaded for point cloud with id %zu", vtx_id);
            nob_return_defer(false);
        }
    } else {
        if (vtx_id < point_clouds.count && point_clouds.items[vtx_id].handle) {
            vtx_buff = point_clouds.items[vtx_id];
        } else {
            nob_log(NOB_ERROR, "vertex buffer was not uploaded for point cloud with id %zu", vtx_id);
            nob_return_defer(false);
        }
    }

    Matrix model = {0};
    if (mat_stack_p) {
        model = mat_stack[mat_stack_p - 1];
    } else {
        nob_log(NOB_ERROR, "No matrix stack, cannot draw.");
        nob_return_defer(false);
    }

    Matrix mvp = MatrixMultiply(model, matrices.viewProj);
    if (!vk_draw_points(vtx_buff, mvp, example))
        nob_return_defer(false);

defer:
    return result;
}

static void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void) window;
    Vector2 offsets = {
        .x = x_offset,
        .y = y_offset
    };
    mouse.curr_wheel_move = offsets;
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

int get_fps()
{
    double frame_time = get_frame_time();
    if (frame_time == 0) return 0;
    else return (int)roundf(1.0f / frame_time);
}

void set_target_fps(int fps)
{
    if (fps < 1) cvr_time.target = 0.0;
    else cvr_time.target = 1.0 / (double) fps;
    nob_log(NOB_INFO, "target fps: %02.03f ms", (float) cvr_time.target * 1000.0f);
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
        nob_log(NOB_ERROR, "no matrix available to translate");
}

bool get_matrix_tos(Matrix *model)
{
    bool result = true;

    if (mat_stack_p) {
        *model = mat_stack[mat_stack_p - 1];
    } else {
        nob_log(NOB_ERROR, "No matrix on stack");
        nob_return_defer(false);
    }

defer:
    return result;
}
