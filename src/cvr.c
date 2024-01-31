#include "cvr.h"
#include "vk_ctx.h"
#include <time.h>
#include <vulkan/vulkan_core.h>

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

Keyboard keyboard = {0};
extern Core_State core_state;
extern Vk_Context ctx;
static clock_t time_begin;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void poll_input_events();

bool init_window(int width, int height, const char *title)
{
    bool result = true;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ctx.window = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwSetWindowUserPointer(ctx.window, &ctx);
    glfwSetFramebufferSizeCallback(ctx.window, frame_buff_resized);

    glfwSetKeyCallback(ctx.window, key_callback);

    cvr_chk(cvr_init(), "failed to initialize C Vulkan Renderer");

    time_begin = clock();

    /* Set the default topology to triangle list */
    core_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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
    if (!ctx.pipelines.shape) create_shape_pipeline();
    if (!is_shape_res_alloc(shape_type)) alloc_shape_res(shape_type);
    cvr_chk(cvr_draw_shape(shape_type), "failed to draw frame");

defer:
    return result;
}

void clear_background(Color color)
{
    begin_render_pass(color);
}

void begin_mode_3d(Camera camera)
{
    core_state.camera = camera;
    begin_draw();
}

void end_mode_3d()
{
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

void push_matrix()
{
    if (!cvr_push_matrix()) nob_log(NOB_ERROR, "matrix stack overflow");
}

void pop_matrix()
{
    if (!cvr_pop_matrix()) nob_log(NOB_ERROR, "matrix stack underflow");
}

void translate(float x, float y, float z)
{
    if (!cvr_translate(x, y, z)) nob_log(NOB_ERROR, "no matrix available to translate");
}

void rotate(Vector3 axis, float angle)
{
    if (!cvr_rotate(axis, angle)) nob_log(NOB_ERROR, "no matrix available to rotate");
}

void rotate_x(float angle)
{
    if (!cvr_rotate_x(angle)) nob_log(NOB_ERROR, "no matrix available to rotate x");
}

void rotate_y(float angle)
{
    if (!cvr_rotate_y(angle)) nob_log(NOB_ERROR, "no matrix available to rotate y");
}

void rotate_z(float angle)
{
    if (!cvr_rotate_z(angle)) nob_log(NOB_ERROR, "no matrix available to rotate z");
}

void rotate_xyz(Vector3 angle)
{
    if (!cvr_rotate_xyz(angle)) nob_log(NOB_ERROR, "no matrix available to rotate xyz");
}

void rotate_zyx(Vector3 angle)
{
    if (!cvr_rotate_zyx(angle)) nob_log(NOB_ERROR, "no matrix available to rotate zyx");
}

void scale(float x, float y, float z)
{
    if (!cvr_scale(x, y, z)) nob_log(NOB_ERROR, "no matrix available to scale");
}

void enable_point_topology()
{
    core_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}
