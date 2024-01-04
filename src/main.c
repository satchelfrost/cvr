#include "app.h"
#include "app_utils.h"
#include "ext_man.h"

#define NOB_IMPLEMENTATION
#include "ext/nob.h"
#include "nob_ext.h"

#define RAYMATH_IMPLEMENTATION
#include "ext/raylib-5.0/raymath.h"

bool init_window();
bool main_loop();
static void frame_buff_resized(GLFWwindow* window, int width, int height);

int main()
{
    if (!init_window()) return 1;
    if (!app_ctor()) return 1;
    if (!main_loop()) return 1;
    if (!app_dtor()) return 1;

    return 0;
}

extern App app; // app.c

bool init_window()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    app.window = glfwCreateWindow(800, 600, "C Vulkan Renderer", NULL, NULL);
    glfwSetWindowUserPointer(app.window, &app);
    glfwSetFramebufferSizeCallback(app.window, frame_buff_resized);
    return true;
}

bool main_loop()
{
    bool result = true;
    while(!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        cvr_chk(draw(), "failed to draw frame");
    }

defer:
    vkDeviceWaitIdle(app.device);
    return result;
}

static void frame_buff_resized(GLFWwindow* window, int width, int height)
{
    unused(width);
    unused(height);
    App *app = (App*)(glfwGetWindowUserPointer(window));
    if (app)
        app->swpchain.buff_resized = true;
}