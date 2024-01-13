#ifndef CVR_RENDER_H_
#define CVR_RENDER_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "common.h"
#include "cvr_buffer.h"

typedef struct {
    uint32_t gfx_idx;
    bool has_gfx;
    uint32_t present_idx;
    bool has_present;
} QueueFamilyIndices;

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
    vec(CVR_Buffer) ubos;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    vec(VkDescriptorSet) descriptor_sets;
} App;

/* CVR render functions */
bool app_ctor();
bool app_dtor();
bool create_instance();
bool create_device();
bool create_surface();
bool create_swpchain();
bool create_img_views();
bool create_descriptor_set_layout();
bool create_gfx_pipeline();
bool create_shader_module(const char *shader, VkShaderModule *module);
bool create_render_pass();
bool create_frame_buffs();
bool recreate_swpchain();
bool create_vtx_buffer();
bool create_idx_buffer();
bool create_ubos();
void update_ubos(uint32_t curr_frame);
bool create_descriptor_pool();
bool create_descriptor_sets();
bool draw();
bool rec_cmds(uint32_t img_idx, VkCommandBuffer cmd_buffer);

/* Utilities */
void populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *debug_msgr_ci);
Nob_Log_Level translate_msg_severity(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity);
bool setup_debug_msgr();
QueueFamilyIndices find_queue_fams(VkPhysicalDevice phys_device);
typedef vec(uint32_t) U32_Set;
void populate_set(int arr[], size_t arr_size, U32_Set *set);
bool swpchain_adequate(VkPhysicalDevice phys_device);
VkSurfaceFormatKHR choose_swpchain_fmt();
VkPresentModeKHR choose_present_mode();
VkExtent2D choose_swp_extent();
bool is_device_suitable(VkPhysicalDevice phys_device);
bool pick_phys_device();
void cleanup_swpchain();
bool find_mem_type_idx(uint32_t type, VkMemoryPropertyFlags properties, uint32_t *idx);

#endif // CVR_RENDER_H_
