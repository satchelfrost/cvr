#include "cvr.h"

#define RAYMATH_IMPLEMENTATION
#include "ext/raylib-5.0/raymath.h"

#define VK_CTX_IMPLEMENTATION
#include "vk_ctx.h"

#define NOB_IMPLEMENTATION
#include "ext/nob.h"

#define MAX_KEYBOARD_KEYS 512
#define MAX_KEY_PRESSED_QUEUE 16
#define MAX_CHAR_PRESSED_QUEUE 16

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
    Vk_Buffer vtx_buff;
    Vk_Buffer idx_buff;
    const Vertex *verts;
    size_t vert_count;
    size_t vert_size;
    const uint16_t *idxs;
    size_t idx_count;
    size_t idx_size;
} Shape;

Keyboard keyboard = {0};
static clock_t time_begin;
#define MAX_MAT_STACK 1024 * 1024
Matrix mat_stack[MAX_MAT_STACK];
size_t mat_stack_p = 0;
Shape shapes[SHAPE_COUNT];

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void poll_input_events();

/* Must first set the verts, vert_size, & vert_count */
bool create_shape_vtx_buffer(Shape *shape);

/* Must first set the idxs, idx_size, & idx_count */
bool create_shape_idx_buffer(Shape *shape);

bool alloc_shape_res(Shape_Type shape_type);
bool is_shape_res_alloc(Shape_Type shape_type);

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

    /* Initialize vulkan stuff */
    cvr_chk(cvr_init(), "failed to initialize C Vulkan Renderer");

    /* Start the clock */
    time_begin = clock();

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
    if (!ctx.pipelines.deflt) create_default_pipeline();
    if (!is_shape_res_alloc(shape_type)) alloc_shape_res(shape_type);

    Vk_Buffer vtx_buff = shapes[shape_type].vtx_buff;
    Vk_Buffer idx_buff = shapes[shape_type].idx_buff;
    if (mat_stack_p)
        cvr_chk(draw(ctx.pipelines.deflt, vtx_buff, idx_buff, mat_stack[mat_stack_p - 1]), "failed to draw frame");
    else
        nob_log(NOB_ERROR, "No matrix stack, cannot draw.");

defer:
    return result;
}

void begin_mode_3d(Camera camera)
{
    core_state.camera = camera;
    cvr_set_proj(camera);
    cvr_set_view(camera);
    push_matrix();
}

void begin_drawing(Color color)
{
    begin_draw();
    cvr_begin_render_pass(color);
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

void end_drawing()
{
    cvr_update_ubos();
    end_draw();
    poll_input_events();
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
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    unused(scancode);
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

void set_cube_color(Color color)
{
    core_state.cube_color.x = color.r / (float)255.0f;
    core_state.cube_color.y = color.g / (float)255.0f;
    core_state.cube_color.z = color.b / (float)255.0f;
}

double get_time()
{
    clock_t curr_time = clock();
    return (double)(curr_time - time_begin) / CLOCKS_PER_SEC;
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

void enable_point_topology()
{
    core_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

bool create_shape_vtx_buffer(Shape *shape)
{
    bool result = true;

    /* create two buffers, a so-called "staging" buffer, and one for our actual vertex buffer */
    Vk_Buffer stg_buff;
    shape->vtx_buff.device = stg_buff.device = ctx.device;
    shape->vtx_buff.size   = stg_buff.size   = shape->vert_size * shape->vert_count;
    shape->vtx_buff.count  = stg_buff.count  = shape->vert_count;
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    void* data;
    vk_chk(vkMapMemory(ctx.device, stg_buff.buff_mem, 0, stg_buff.size, 0, &data), "failed to map memory");
    memcpy(data, shape->verts, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.buff_mem);

    result = vk_buff_init(
        &shape->vtx_buff,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create vertex buffer");

    /* transfer data from staging buffer to vertex buffer */
    vk_buff_copy(&cmd_man, ctx.gfx_queue, stg_buff, shape->vtx_buff, 0);

defer:
    vk_buff_destroy(stg_buff);
    return result;
}

bool create_shape_idx_buffer(Shape *shape)
{
    bool result = true;

    /* create two buffers, a so-called "staging" buffer, and one for our actual index buffer */
    Vk_Buffer stg_buff;
    shape->idx_buff.device = stg_buff.device = ctx.device;
    shape->idx_buff.size   = stg_buff.size   = shape->idx_size * shape->idx_count;
    shape->idx_buff.count  = stg_buff.count  = shape->idx_count;
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    void* data;
    vk_chk(vkMapMemory(ctx.device, stg_buff.buff_mem, 0, stg_buff.size, 0, &data), "failed to map memory");
    memcpy(data, shape->idxs, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.buff_mem);

    result = vk_buff_init(
        &shape->idx_buff,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create index buffer");

    /* transfer data from staging buffer to index buffer */
    vk_buff_copy(&cmd_man, ctx.gfx_queue, stg_buff, shape->idx_buff, 0);

defer:
    vk_buff_destroy(stg_buff);
    return result;
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
#define X(CONST_NAME, array_name)                        \
    case SHAPE_ ## CONST_NAME:                           \
    shape->vert_count = CONST_NAME ## _VERTS;            \
    shape->vert_size = sizeof(array_name ## _verts[0]);  \
    shape->verts = array_name ## _verts;                 \
    shape->idx_count = CONST_NAME ## _IDXS;              \
    shape->idx_size = sizeof(array_name ## _indices[0]); \
    shape->idxs = array_name ## _indices;                \
    break;
    SHAPE_LIST
#undef X
    default:
        nob_log(NOB_ERROR, "unrecognized shape %d", shape_type);
        nob_return_defer(false);
    }

    if ((!create_shape_vtx_buffer(shape)) || (!create_shape_idx_buffer(shape))) {
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
    cvr_destroy();
    glfwDestroyWindow(ctx.window);
    glfwTerminate();
}
