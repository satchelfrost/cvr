#ifndef APP_H_
#define APP_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "common.h"

typedef struct {
    VkBuffer buff;
    VkDeviceMemory buff_mem;
} CVR_Buffer;

typedef struct {
    VkSwapchainKHR handle;
    vec(VkImage) imgs;
    vec(VkImageView) img_views;
    vec(VkFramebuffer) buffs;
    bool buff_resized;
} CVR_Swpchain;

typedef struct {
    VkCommandPool pool;
    vec(VkCommandBuffer) buffs;
    vec(VkSemaphore) img_avail_sems;
    vec(VkSemaphore) render_finished_sems;
    vec(VkFence) fences;
} CVR_Cmd;

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
    CVR_Cmd cmd;
    CVR_Buffer vtx;
    CVR_Buffer idx;
    CVR_Buffer stg;
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
bool recreate_swpchain();
bool create_cvr_buffer(
    CVR_Buffer *buffer,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
);
bool create_vtx_buffer();
void destroy_buffer(CVR_Buffer buffer);
void destroy_cmd(CVR_Cmd cmd);

bool draw();
bool rec_cmds(uint32_t img_idx, VkCommandBuffer cmd_buffer);

#endif // APP_H_