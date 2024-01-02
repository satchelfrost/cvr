#include "app.h"
#include "app_utils.h"
#include "ext_man.h"

#define NOB_IMPLEMENTATION
#include "ext/nob.h"
#include "nob_ext.h"

#define RAYMATH_IMPLEMENTATION
#include "ext/raylib-5.0/raymath.h"

bool init_window();
bool init_vulkan();
bool main_loop();
bool cleanup();
static void frame_buff_resized(GLFWwindow* window, int width, int height);

int main()
{
    if (!init_window()) return 1;
    if (!init_vulkan()) return 1;
    if (!main_loop()) return 1;
    if (!cleanup()) return 1;

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

bool init_vulkan()
{
    bool result = true;
    init_ext_managner();
    cvr_chk(create_instance(), "failed to create instance");
#ifdef ENABLE_VALIDATION
    cvr_chk(setup_debug_msgr(), "failed to setup debug messenger");
#endif
    cvr_chk(create_surface(), "failed to create vulkan surface");
    cvr_chk(pick_phys_device(), "failed to find suitable GPU");
    cvr_chk(create_device(), "failed to create logical device");
    cvr_chk(create_swpchain(), "failed to create swapchain");
    cvr_chk(create_img_views(), "failed to create image views");
    cvr_chk(create_render_pass(), "failed to create render pass");
    cvr_chk(create_gfx_pipeline(), "failed to create graphics pipelin");
    cvr_chk(create_frame_buffs(), "failed to create frame buffers");
    cvr_chk(create_cmd_pool(), "failed to create command pool");
    cvr_chk(create_vtx_buffer(), "failed to create vertex buffer");
    cvr_chk(create_cmd_buff(), "failed to create command buffers");
    cvr_chk(create_syncs(), "failed to create synchronization objects");

defer:
    return result;
}

bool cleanup()
{
    cleanup_swpchain();
    destroy_buffer(app.vtx);
    destroy_cmd(app.cmd);
    vkDestroyPipeline(app.device, app.pipeline, NULL);
    vkDestroyPipelineLayout(app.device, app.pipeline_layout, NULL);
    vkDestroyRenderPass(app.device, app.render_pass, NULL);
    vkDestroyDevice(app.device, NULL);
#ifdef ENABLE_VALIDATION
    load_pfn(vkDestroyDebugUtilsMessengerEXT);
    if (vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(app.instance, app.debug_msgr, NULL);
#endif
    vkDestroySurfaceKHR(app.instance, app.surface, NULL);
    vkDestroyInstance(app.instance, NULL);
    glfwDestroyWindow(app.window);
    glfwTerminate();
    destroy_ext_manager();
    return true;
}

static void frame_buff_resized(GLFWwindow* window, int width, int height)
{
    unused(width);
    unused(height);
    App *app = (App*)(glfwGetWindowUserPointer(window));
    app->swpchain.buff_resized = true;
}
