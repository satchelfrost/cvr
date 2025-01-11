#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow *handle;
} Platform_Data;

static Platform_Data platform = {0};

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset);
static void joystick_callback(int jid, int event);
static void frame_buff_resized(GLFWwindow* window, int width, int height);

bool init_platform()
{
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (full_screen) {
        int monitor_count = 0;
        int largest_monitor_idx = 0;
        int highest_pixel_count = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
        for (int i = 0; i < monitor_count; i++) {
            const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
            int pixel_count = mode->width * mode->height;
            if (i == 0) {
                highest_pixel_count = pixel_count;
            } else if (pixel_count > highest_pixel_count) {
                largest_monitor_idx = i;
                highest_pixel_count = pixel_count;
            }
        }
        GLFWmonitor *largest_monitor = monitors[largest_monitor_idx];
        const char *name = glfwGetMonitorName(largest_monitor);
        const GLFWvidmode *mode = glfwGetVideoMode(largest_monitor);
        win_size.width = mode->width;
        win_size.height = mode->height;
        vk_log(VK_INFO, "full screen mode enabled", mode->width, mode->height);
        vk_log(VK_INFO, "monitor %s, (width, hieght) = (%d, %d)", name, mode->width, mode->height);
        platform.handle = glfwCreateWindow(win_size.width, win_size.height, core_title, largest_monitor, NULL);
        if (!platform.handle) return false;
    } else {
        platform.handle = glfwCreateWindow(win_size.width, win_size.height, core_title, NULL, NULL);
        if (!platform.handle) return false;
    }
    glfwSetWindowUserPointer(platform.handle, &vk_ctx);
    glfwSetFramebufferSizeCallback(platform.handle, frame_buff_resized);
    glfwSetKeyCallback(platform.handle, key_callback);
    glfwSetMouseButtonCallback(platform.handle, mouse_button_callback);
    glfwSetCursorPosCallback(platform.handle, mouse_cursor_pos_callback);
    glfwSetScrollCallback(platform.handle, mouse_scroll_callback);
    glfwSetJoystickCallback(joystick_callback);

    return true;
}

bool platform_surface_init()
{
    if (VK_SUCCEEDED(glfwCreateWindowSurface(vk_ctx.instance, platform.handle, NULL, &vk_ctx.surface))) {
        return true;
    } else {
        vk_log(VK_ERROR, "failed to initialize glfw window surface");
        return false;
    }
}

bool window_should_close()
{
    bool result = glfwWindowShouldClose(platform.handle) || is_key_down(KEY_ESCAPE);
    glfwPollEvents();
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

void poll_input_events()
{
    /* keyboard */
    keyboard.key_pressed_queue_count = 0;
    keyboard.char_pressed_queue_count = 0;
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) {
        keyboard.prev_key_state[i] = keyboard.curr_key_state[i];
        keyboard.key_repeat_in_frame[i] = 0;
    }

    /* mouse */
    mouse.prev_pos = mouse.curr_pos;
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
        mouse.prev_button_state[i] = mouse.curr_button_state[i];
    mouse.prev_wheel_move = mouse.curr_wheel_move;
    mouse.curr_wheel_move = (Vector2){ 0.0f, 0.0f };

    /* gamepad */
    GLFWgamepadstate state = {0};
    glfwGetGamepadState(0, &state); // 0 means only one controller is supported

    /* gamepad buttons */
    for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
        gamepad.prev_button_state[i] = gamepad.curr_button_state[i];
    const unsigned char *buttons = state.buttons;
    for (int i = 0; buttons != NULL && i < GLFW_GAMEPAD_BUTTON_DPAD_LEFT + 1 && i < MAX_GAMEPAD_BUTTONS; i++) {
        int btn = -1;

        switch (i) {
        case GLFW_GAMEPAD_BUTTON_Y:            btn = GAMEPAD_BUTTON_RIGHT_FACE_UP;    break;
        case GLFW_GAMEPAD_BUTTON_B:            btn = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT; break;
        case GLFW_GAMEPAD_BUTTON_A:            btn = GAMEPAD_BUTTON_RIGHT_FACE_DOWN;  break;
        case GLFW_GAMEPAD_BUTTON_X:            btn = GAMEPAD_BUTTON_RIGHT_FACE_LEFT;  break;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:  btn = GAMEPAD_BUTTON_LEFT_TRIGGER_1;   break;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: btn = GAMEPAD_BUTTON_RIGHT_TRIGGER_1;  break;
        case GLFW_GAMEPAD_BUTTON_BACK:         btn = GAMEPAD_BUTTON_MIDDLE_LEFT;      break;
        case GLFW_GAMEPAD_BUTTON_GUIDE:        btn = GAMEPAD_BUTTON_MIDDLE;           break;
        case GLFW_GAMEPAD_BUTTON_START:        btn = GAMEPAD_BUTTON_MIDDLE_RIGHT;     break;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP:      btn = GAMEPAD_BUTTON_LEFT_FACE_UP;     break;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:   btn = GAMEPAD_BUTTON_LEFT_FACE_RIGHT;  break;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:    btn = GAMEPAD_BUTTON_LEFT_FACE_DOWN;   break;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:    btn = GAMEPAD_BUTTON_LEFT_FACE_LEFT;   break;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB:   btn = GAMEPAD_BUTTON_LEFT_THUMB;       break;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB:  btn = GAMEPAD_BUTTON_RIGHT_THUMB;      break;
        default: break;
        }

        if (btn != -1) {
            if (buttons[i] == GLFW_PRESS) {
                gamepad.curr_button_state[btn] = 1;
                gamepad.last_button_pressed = btn;
            } else {
                gamepad.curr_button_state[btn] = 0;
            }
        }
    }

    /* gamepad axes */
    const float *axes = state.axes;
    for (int i = 0; axes != NULL && i < GLFW_GAMEPAD_AXIS_LAST + 1 && i < MAX_GAMEPAD_AXIS; i++)
        gamepad.axis_state[i] = axes[i];

    /* if we want to treat trigger buttons as booleans */
    gamepad.curr_button_state[GAMEPAD_BUTTON_LEFT_TRIGGER_2]  = gamepad.axis_state[GAMEPAD_AXIS_LEFT_TRIGGER] > 0.1f;
    gamepad.curr_button_state[GAMEPAD_BUTTON_RIGHT_TRIGGER_2] = gamepad.axis_state[GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.1f;
}

double get_time()
{
    return glfwGetTime();
}

void close_platform()
{
    glfwDestroyWindow(platform.handle);
    glfwTerminate();
}

void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window;
    Vector2 offsets = {.x = x_offset, .y = y_offset};
    mouse.curr_wheel_move = offsets;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    (void) window;
    (void) mods;
    mouse.curr_button_state[button] = action;
}

void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y)
{
    (void) window;

    mouse.curr_pos.x = x;
    mouse.curr_pos.y = y;
}

void joystick_callback(int jid, int event)
{
    if (event == GLFW_CONNECTED) {
        vk_log(VK_INFO, "Connected jid %d, event %d, name %s", jid, event, glfwGetJoystickName(jid));
    }
    else if (event == GLFW_DISCONNECTED) {
        vk_log(VK_INFO, "Disconnected jid %d, event %d, name %s", jid, event, glfwGetJoystickName(jid));
    }
}

void set_window_size(int width, int height)
{
    win_size.width = width;
    win_size.height = height;
    glfwSetWindowSize(platform.handle, width, height);
}

void set_window_pos(int x, int y)
{
    glfwSetWindowPos(platform.handle, x, y);
}

void frame_buff_resized(GLFWwindow* window, int width, int height)
{
    (void)window;
    win_size.width = width;
    win_size.height = height;
    vk_ctx.swapchain.buff_resized = true;
}

const char **get_platform_exts(uint32_t *platform_ext_count)
{
    return glfwGetRequiredInstanceExtensions(platform_ext_count);
}

void platform_wait_resize_frame_buffer()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(platform.handle, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(platform.handle, &width, &height);
        glfwWaitEvents();
    }
}

void platform_frame_buff_size(int *width, int *height)
{
    glfwGetFramebufferSize(platform.handle, width, height);
}
