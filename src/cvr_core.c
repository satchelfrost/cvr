#include "cvr.h"
#include "cvr_context.h"
#include "cvr_window.h"
#include "ext/raylib-5.0/raylib.h"

#define NOB_IMPLEMENTATION
#include "ext/nob.h"

extern CVR_Window window; // cvr_window.c
extern CVR_Context ctx;   // cvr_context.c

bool init_window(int width, int height, const char *title)
{
    bool result = true;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.handle = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwSetWindowUserPointer(window.handle, &ctx);
    glfwSetFramebufferSizeCallback(window.handle, frame_buff_resized);

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

void begin_mode_3d(Camera3D camera)
{
    ctx.state.camera = camera;
}
