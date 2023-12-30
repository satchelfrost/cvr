#include "app.h"
#include "app_utils.h"
#include "ext_man.h"

#define NOB_IMPLEMENTATION
#include "nob.h"
#include "nob_ext.h"

bool init_window();
bool init_vulkan();
bool main_loop();
bool cleanup();

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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    app.window = glfwCreateWindow(800, 600, "C Vulkan Renderer", NULL, NULL);
    return true;
}

bool main_loop()
{
    bool result = true;
    while(!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        CVR_CHK(draw(), "failed to draw frame");
    }

defer:
    vkDeviceWaitIdle(app.device);
    return result;
}

bool init_vulkan()
{
    bool result = true;
    init_ext_managner();
    CVR_CHK(create_instance(), "failed to create instance");
#ifdef ENABLE_VALIDATION
    CVR_CHK(setup_debug_msgr(), "failed to setup debug messenger");
#endif
    CVR_CHK(create_surface(), "failed to create vulkan surface");
    CVR_CHK(pick_phys_device(), "failed to find suitable GPU");
    CVR_CHK(create_device(), "failed to create logical device");
    CVR_CHK(create_swpchain(), "failed to create swapchain");
    CVR_CHK(create_img_views(), "failed to create image views");
    CVR_CHK(create_render_pass(), "failed to create render pass");
    CVR_CHK(create_gfx_pipeline(), "failed to create graphics pipelin");
    CVR_CHK(create_frame_buffs(), "failed to create frame buffers");
    CVR_CHK(create_cmd_pool(), "failed to create command pool");
    CVR_CHK(create_cmd_buff(), "failed to create command buffers");
    CVR_CHK(create_syncs(), "failed to create synchronization objects");

defer:
    return result;
}

bool cleanup()
{
    for (size_t i = 0; i < app.img_available_sems.count; i++)
        vkDestroySemaphore(app.device, app.img_available_sems.items[i], NULL);
    for (size_t i = 0; i < app.render_finished_sems.count; i++)
        vkDestroySemaphore(app.device, app.render_finished_sems.items[i], NULL);
    for (size_t i = 0; i < app.fences.count; i++)
        vkDestroyFence(app.device, app.fences.items[i], NULL);
    vkDestroyCommandPool(app.device, app.cmd_pool, NULL);
    for (size_t i = 0; i < app.frame_buffs.count; i++)
        vkDestroyFramebuffer(app.device, app.frame_buffs.items[i], NULL);
    nob_da_free(app.frame_buffs);
    vkDestroyPipeline(app.device, app.pipeline, NULL);
    vkDestroyPipelineLayout(app.device, app.pipeline_layout, NULL);
    vkDestroyRenderPass(app.device, app.render_pass, NULL);
    for (size_t i = 0; i < app.swpchain_img_views.count; i++)
        vkDestroyImageView(app.device, app.swpchain_img_views.items[i], NULL);
    nob_da_free(app.swpchain_img_views);
    vkDestroySwapchainKHR(app.device, app.swpchain, NULL);
    nob_da_free(app.swpchain_imgs);
    vkDestroyDevice(app.device, NULL);
#ifdef ENABLE_VALIDATION
    LOAD_PFN(vkDestroyDebugUtilsMessengerEXT);
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