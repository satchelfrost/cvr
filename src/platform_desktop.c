#include <GLFW/glfw3.h>

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset);
static void joystick_callback(int jid, int event);
static void frame_buff_resized(GLFWwindow* window, int width, int height);
void poll_input_events();
float get_mouse_wheel_move();

void init_platform()
{
    /* Initialize glfw stuff */
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ctx.window = glfwCreateWindow(win_size.width, win_size.height, core_title, NULL, NULL);
    glfwSetWindowUserPointer(ctx.window, &ctx);
    glfwSetFramebufferSizeCallback(ctx.window, frame_buff_resized);
    glfwSetKeyCallback(ctx.window, key_callback);
    glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
    glfwSetCursorPosCallback(ctx.window, mouse_cursor_pos_callback);
    glfwSetScrollCallback(ctx.window, mouse_scroll_callback);
    glfwSetJoystickCallback(joystick_callback);
}

bool window_should_close()
{
    bool result = glfwWindowShouldClose(ctx.window);
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

float get_mouse_wheel_move()
{
    float result = 0.0f;

    if (fabsf(mouse.curr_wheel_move.x) > fabsf(mouse.curr_wheel_move.y)) result = (float)mouse.curr_wheel_move.x;
    else result = (float)mouse.curr_wheel_move.y;

    return result;
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
    glfwDestroyWindow(ctx.window);
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
        nob_log(NOB_INFO, "Connected jid %d, event %d, name %s", jid, event, glfwGetJoystickName(jid));
    }
    else if (event == GLFW_DISCONNECTED) {
        nob_log(NOB_INFO, "Disconnected jid %d, event %d, name %s", jid, event, glfwGetJoystickName(jid));
    }
}

void set_window_size(int width, int height)
{
    win_size.width = width;
    win_size.height = height;
    glfwSetWindowSize(ctx.window, width, height);
}

void set_window_pos(int x, int y)
{
    glfwSetWindowPos(ctx.window, x, y);
}

void frame_buff_resized(GLFWwindow* window, int width, int height)
{
    (void)window;
    win_size.width = width;
    win_size.height = height;
    ctx.swapchain.buff_resized = true;
}

