#ifndef VK_CTX_H_
#define VK_CTX_H_

#include "common.h"
#include "vk_buffer.h"
#include "vertex.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdint.h>
#include <sys/types.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
} Vk_Swpchain;

#ifndef SHAPE_TYPE
typedef enum {
    SHAPE_CUBE = 0,
    SHAPE_QUAD,
    SHAPE_TETRAHEDRON,
    SHAPE_COUNT,
} Shape_Type;
#endif

typedef struct {
    Vk_Buffer vtx_buff;
    Vk_Buffer idx_buff;
    const Vertex *verts;
    size_t vert_count;
    size_t vert_size;
    const uint16_t *idxs;
    size_t idx_count;
    size_t idx_size;
} Shape;

/* Vulkan Context */
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
    Vk_Swpchain swpchain;
    Shape shapes[SHAPE_COUNT];
    vec(Vk_Buffer) ubos;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    vec(VkDescriptorSet) descriptor_sets;
    struct {
        VkPipeline shape;
    } pipelines;
} Vk_Context;

/* CVR render functions */
bool cvr_init();
bool cvr_destroy();
bool create_instance();
bool create_device();
bool create_surface();
bool create_swpchain();
bool create_img_views();
bool create_descriptor_set_layout();
bool create_shape_pipeline();
bool create_shader_module(const char *shader, VkShaderModule *module);
bool create_render_pass();
bool create_frame_buffs();
bool recreate_swpchain();

/* Must first set the verts, vert_size, & vert_count */
bool create_shape_vtx_buffer(Shape *shape);

/* Must first set the idxs, idx_size, & idx_count */
bool create_shape_idx_buffer(Shape *shape);

bool alloc_shape_res(Shape_Type shape_type);
bool create_ubos();
void update_ubos(uint32_t curr_frame);
bool create_descriptor_pool();
bool create_descriptor_sets();

/* Manages synchronization info and gets ready for vulkan commands.
 * Returns true if succeeded and false otherwise */
bool begin_draw();

/* Submits vulkan commands.
 * Returns true if succeeded and false otherwise */
bool end_draw();

bool cvr_draw_shape(Shape_Type shape_type);
bool rec_cmds(VkCommandBuffer cmd_buffer, Shape_Type shape_type);

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
void frame_buff_resized(GLFWwindow* window, int width, int height);
bool is_shape_res_alloc(Shape_Type shape_type);

#ifndef CVR_CAMERA
typedef struct {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    float fovy;
    int projection;
} Camera;
#endif

#ifndef CVR_COLOR
typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;
#endif

typedef struct {
    Color clear_color;
    Vector3 cube_color;
    Camera camera;
} Core_State;

#endif // VK_CTX_H_
