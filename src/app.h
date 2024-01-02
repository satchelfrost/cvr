#ifndef APP_H_
#define APP_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "common.h"
#include "cvr_buffer.h"

typedef struct {
    VkSwapchainKHR handle;
    vec(VkImage) imgs;
    vec(VkImageView) img_views;
    vec(VkFramebuffer) buffs;
    bool buff_resized;
} CVR_Swpchain;

typedef struct {
    GLFWwindow *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_msgr;
    VkPhysicalDevice phys_device;
    VkDevice device;
    VkQueue gfx_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_fmt;
    VkExtent2D extent;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    CVR_Swpchain swpchain;
    CVR_Buffer vtx;
    CVR_Buffer idx;
} App;

bool app_ctor();
bool app_dtor();
bool create_instance();
bool create_device();
bool create_surface();
bool create_swpchain();
bool create_img_views();
bool create_gfx_pipeline();
bool create_shader_module(const char *shader, VkShaderModule *module);
bool create_render_pass();
bool create_frame_buffs();
bool recreate_swpchain();
bool create_vtx_buffer();
bool create_idx_buffer();

bool draw();
bool rec_cmds(uint32_t img_idx, VkCommandBuffer cmd_buffer);

#endif // APP_H_