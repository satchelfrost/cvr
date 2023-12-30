#ifndef APP_H_
#define APP_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "common.h"

/* Data for main vulkan application */

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
    VkSwapchainKHR swpchain;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool cmd_pool;
    Vec(VkCommandBuffer) cmd_buffers;
    Vec(VkSemaphore) img_available_sems;
    Vec(VkSemaphore) render_finished_sems;
    Vec(VkFence) fences;
    Vec(VkImage) swpchain_imgs;
    Vec(VkImageView) swpchain_img_views;
    Vec(VkFramebuffer) frame_buffs;
} App;

bool create_instance();
bool create_device();
bool create_surface();
bool create_swpchain();
bool create_img_views();
bool create_gfx_pipeline();
bool create_shader_module(const char *shader, VkShaderModule *module);
bool create_render_pass();
bool create_frame_buffs();
bool create_cmd_pool();
bool create_cmd_buff();
bool create_syncs();

bool draw();
bool rec_cmds(uint32_t img_idx, VkCommandBuffer cmd_buffer);

#endif // APP_H_