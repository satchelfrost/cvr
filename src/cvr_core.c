#include "cvr.h"
#include "cvr_context.h"
#include "cvr_window.h"

#define NOB_IMPLEMENTATION
#include "ext/nob.h"

extern CVR_Window window; // cvr_window.c
extern CVR_Context ctx;   // cvr_context.c

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

static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

bool init_window(int width, int height, const char *title)
{
    bool result = true;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.handle = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwSetWindowUserPointer(window.handle, &ctx);
    glfwSetFramebufferSizeCallback(window.handle, frame_buff_resized);

    glfwSetKeyCallback(window.handle, KeyCallback);

    cvr_chk(cvr_init(), "failed to initialize C Vulkan Renderer");

defer:
    return result;
}

bool window_should_close()
{
    bool result = glfwWindowShouldClose(window.handle);
    glfwPollEvents();
    return result;
}

bool draw()
{
    bool result = true;
    cvr_chk(cvr_draw(), "failed to draw frame");
defer:
    return result;
}

void clear_background(Color color)
{
    ctx.state.clear_color = color;
}

void begin_mode_3d(CVR_Camera camera)
{
    ctx.state.camera = camera;
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

void poll_input_events()
{
    keyboard.key_pressed_queue_count = 0;
    keyboard.char_pressed_queue_count = 0;

    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) {
        keyboard.prev_key_state[i] = keyboard.curr_key_state[i];
        keyboard.key_repeat_in_frame[i] = 0;
    }
}

static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
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
