#include "cvr_window.h"
#include "cvr_context.h"

extern CVR_Context ctx; // cvr_context.c
CVR_Window window = {0};

void frame_buff_resized(GLFWwindow* window, int width, int height)
{
    unused(window);
    unused(width);
    unused(height);
    ctx.swpchain.buff_resized = true;
}

void close_window()
{
    cvr_destroy();
    glfwDestroyWindow(window.handle);
    glfwTerminate();
}
