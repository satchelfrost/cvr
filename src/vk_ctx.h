#ifndef VK_CTX_H_
#define VK_CTX_H_

#include "cvr.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdint.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>
#include "geometry.h"
#include "ext/nob.h"
#include "nob_ext.h"

#define GLFW_INCLUDE_VULKAN // TODO: would like to move this into cvr.c
#include <GLFW/glfw3.h>

/* Common macro definitions */
#define load_pfn(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(ctx.instance, #pfn)
#define MIN_SEVERITY NOB_WARNING
#define VK_SUCCEEDED(x) ((x) == VK_SUCCESS)
// TODO: get rid of cvr_chk
#define cvr_chk(expr, msg)           \
    do {                             \
        if (!(expr)) {               \
            nob_log(NOB_ERROR, msg); \
            nob_return_defer(false); \
        }                            \
    } while (0)

/* TODO: get rid of vk_chk */
#define vk_chk(vk_result, msg) cvr_chk(VK_SUCCEEDED(vk_result), msg)
#define clamp(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))

typedef struct {
    uint32_t gfx_idx;
    bool has_gfx;
    uint32_t present_idx;
    bool has_present;
    uint32_t compute_idx;
    bool has_compute;
} Queue_Fams;

typedef struct {
    VkImage *items;
    size_t count;
    size_t capacity;
} Imgs;

typedef struct {
    VkImageView *items;
    size_t count;
    size_t capacity;
} Img_Views;

typedef struct {
    VkFramebuffer *items;
    size_t count;
    size_t capacity;
} Frame_Buffs;

typedef struct {
    VkSwapchainKHR handle;
    Imgs imgs;
    Img_Views img_views;
    Frame_Buffs buffs;
    bool buff_resized;
} Vk_Swapchain;

typedef struct {
    size_t size;
    size_t count;
    VkBuffer handle;
    VkDeviceMemory mem;
    void *mapped;
} Vk_Buffer;

typedef struct {
    VkExtent2D extent;
    VkImage handle;
    VkDeviceMemory mem;
    VkImageAspectFlags aspect_mask;
    VkFormat format;
} Vk_Image;

typedef struct {
    VkImageView view;
    VkSampler sampler;
    Vk_Image img;
    size_t id;
    bool active; // TODO: I don't like this
    VkDescriptorSet descriptor_set;
} Vk_Texture;

typedef struct {
    Vk_Texture *items;
    size_t count;
    size_t capacity;
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
} Vk_Texture_Set;

typedef struct {
    Vk_Buffer buff;
    void *data;
    VkDescriptorSet descriptor_set;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout set_layout;
} UBO;

typedef struct {
    VkDescriptorSetLayoutBinding *items;
    size_t count;
    size_t capacity;
} Descriptor_Set_Layout_Bindings;

typedef enum {
    PIPELINE_DEFAULT = 0,
    PIPELINE_WIREFRAME,
    PIPELINE_POINT_CLOUD,
    PIPELINE_POINT_CLOUD_ADV,
    PIPELINE_TEXTURE,
    PIPELINE_COMPUTE,
    PIPELINE_SST,
    PIPELINE_COUNT,
} Pipeline_Type;

typedef enum {
    DS_TYPE_TEX,
    DS_TYPE_ADV_POINT_CLOUD,
    DS_TYPE_COMPUTE,
    DS_TYPE_COMPUTE_RASTERIZER,
    DS_TYPE_COUNT,
} Descriptor_Type;

typedef struct {
    VkDescriptorSetLayout *items;
    size_t count;
    size_t capacity;
} Descriptor_Set_Layouts;

typedef struct {
    Vk_Buffer *items;
    size_t count;
    size_t capacity;
    VkDescriptorSet descriptor_set;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout set_layout;
} SSBO_Set;

typedef struct {
    VkPipeline *items;
    size_t count;
    size_t capacity;
} Vk_Pipeline_Set;

typedef struct {
    VkPipelineLayout *items;
    size_t count;
    size_t capacity;
} Vk_Pipeline_Layout_Set;

typedef struct {
    GLFWwindow *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_msgr;
    VkPhysicalDevice phys_device;
    VkDevice device;
    VkQueue gfx_queue;
    VkQueue compute_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_fmt;
    VkExtent2D extent;
    VkRenderPass render_pass;
    Vk_Swapchain swapchain;
    Vk_Image depth_img;
    VkImageView depth_img_view;
    VkPipelineLayout pipeline_layouts[PIPELINE_COUNT];
    VkPipeline pipelines[PIPELINE_COUNT];
    Vk_Pipeline_Set compute_pl_sets[DS_TYPE_COUNT];
    Vk_Pipeline_Layout_Set compute_pl_layout_sets[DS_TYPE_COUNT];
    Vk_Texture_Set texture_sets[DS_TYPE_COUNT];
    UBO ubos[DS_TYPE_COUNT];
    SSBO_Set ssbo_sets[DS_TYPE_COUNT];
} Vk_Context;

bool vk_init();
bool vk_destroy();
bool vk_instance_init();
bool vk_device_init();
bool vk_surface_init();
bool vk_swapchain_init();
bool vk_img_views_init();
bool vk_img_view_init(Vk_Image img, VkImageView *img_view);
bool vk_basic_pl_init(Pipeline_Type pipeline_type);
bool vk_basic_pl_init2(VkPipelineLayout pl_layout, VkPipeline *pl);
bool vk_compute_pl_init(Descriptor_Type ds_type);
bool vk_shader_mod_init(const char *file_name, VkShaderModule *module);
bool vk_render_pass_init();
bool vk_frame_buffs_init();
bool vk_recreate_swapchain();
bool vk_depth_init();
void cleanup_pipeline(VkPipeline pipeline, VkPipelineLayout pl_layout);
bool vk_pl_layout_init(VkDescriptorSetLayout layout, VkPipelineLayout *pl_layout);
bool vk_compute_pl_init2(const char *shader_name, VkPipelineLayout pl_layout, VkPipeline *pipeline);

/* general ubo initializer */
bool vk_ubo_init(UBO ubo, Descriptor_Type type);
bool vk_ubo_init2(UBO *ubo);
bool vk_ubo_descriptor_set_layout_init(VkShaderStageFlags flags, Descriptor_Type ds_type);
bool vk_ubo_descriptor_pool_init(Descriptor_Type ds_type);
bool vk_ubo_descriptor_set_init(Descriptor_Type ds_type);
bool vk_sampler_descriptor_set_layout_init(Descriptor_Type ds_type, VkShaderStageFlags flags);
bool vk_sampler_descriptor_pool_init(Descriptor_Type ds_type);
bool vk_sampler_descriptor_set_init(Descriptor_Type ds_type);
bool vk_ssbo_descriptor_set_layout_init(Descriptor_Type ds_type);
bool vk_ssbo_descriptor_pool_init(Descriptor_Type ds_type);
bool vk_ssbo_descriptor_set_init(Descriptor_Type ds_type);

bool vk_begin_drawing(); /* Begins recording drawing commands */
bool vk_end_drawing();   /* Submits drawing commands. */
bool vk_draw(Pipeline_Type pipeline_type, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix mvp);
bool vk_draw_points(Vk_Buffer vtx_buff, Matrix mvp, Example example);
bool vk_draw_texture(size_t id, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix mvp);

void vk_compute(Descriptor_Type ds_type); // TDOD: get rid of this after porting old compute example
bool vk_begin_compute();                  // TODO: get rid of this after porting old compute example
bool vk_end_compute();                    // TODO: get rid of this after porting old compute example
bool vk_rec_compute();
bool vk_submit_compute();
bool vk_end_rec_compute();
void vk_compute2(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds, size_t x, size_t y, size_t z);
void vk_compute_pl_barrier();
void vk_gfx(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds);

bool vk_buff_init(Vk_Buffer *buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
void vk_buff_destroy(Vk_Buffer buffer);

/* A staged upload to the GPU requires a copy from a host (CPU) visible buffer
 * to a device (GPU) visible buffer. It's slower to upload, because of the copy
 * but should be faster at runtime (with the exception of integrated graphics)*/
bool vk_vtx_buff_upload(Vk_Buffer *vtx_buff, const void *data);
bool vk_vtx_buff_staged_upload(Vk_Buffer *vtx_buff, const void *data);
bool vk_comp_buff_staged_upload(Vk_Buffer *vtx_buff, const void *data);
bool vk_idx_buff_upload(Vk_Buffer *idx_buff, const void *data);
bool vk_idx_buff_staged_upload(Vk_Buffer *idx_buff, const void *data);

/* Copies "size" bytes from src to dst buffer, a value of zero implies copying the whole src buffer */
bool vk_buff_copy(Vk_Buffer dst_buff, Vk_Buffer src_buff, VkDeviceSize size);

bool vk_create_storage_img(Vk_Texture *texture);
void vk_unload_texture2(Vk_Texture texture);

/* descriptor set macros */
#define DS_POOL(DS_TYPE, COUNT)             \
    .type = VK_DESCRIPTOR_TYPE_ ## DS_TYPE, \
    .descriptorCount = COUNT,

#define DS_BINDING(BINDING, DS_TYPE, BIT_FLAGS)       \
    .binding = BINDING,                               \
    .descriptorCount = 1,                             \
    .descriptorType = VK_DESCRIPTOR_TYPE_ ## DS_TYPE, \
    .stageFlags = VK_SHADER_STAGE_ ## BIT_FLAGS,

#define DS_ALLOC(LAYOUTS, COUNT, POOL)                       \
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, \
    .descriptorPool = POOL,                                  \
    .descriptorSetCount = COUNT,                             \
    .pSetLayouts = LAYOUTS,

#define DS_WRITE_BUFF(BINDING, TYPE, SET, BUFF_INFO)  \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  \
    .dstSet = SET,                                    \
    .dstBinding = BINDING,                            \
    .dstArrayElement = 0,                             \
    .descriptorType = VK_DESCRIPTOR_TYPE_ ## TYPE,    \
    .descriptorCount = 1,                             \
    .pBufferInfo = BUFF_INFO,                         \

#define DS_WRITE_IMG(BINDING, TYPE, SET, IMG_INFO)    \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  \
    .dstSet = SET,                                    \
    .dstBinding = BINDING,                            \
    .dstArrayElement = 0,                             \
    .descriptorType = VK_DESCRIPTOR_TYPE_ ## TYPE,    \
    .descriptorCount = 1,                             \
    .pImageInfo = IMG_INFO,                           \

bool vk_create_ds_layout(VkDescriptorSetLayoutCreateInfo layout_ci, VkDescriptorSetLayout *layout);
bool vk_create_ds_pool(VkDescriptorPoolCreateInfo pool_ci, VkDescriptorPool *pool);
void vk_destroy_ds_pool(VkDescriptorPool pool);
void vk_destroy_ds_layout(VkDescriptorSetLayout layout);
bool vk_alloc_ds(VkDescriptorSetAllocateInfo alloc, VkDescriptorSet *sets);
void vk_update_ds(size_t count, VkWriteDescriptorSet *writes);

/* Utilities */
void populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *debug_msgr_ci);
Nob_Log_Level translate_msg_severity(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity);
bool setup_debug_msgr();
Queue_Fams find_queue_fams(VkPhysicalDevice phys_device);

typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} U32_Set;

void populate_set(int arr[], size_t arr_size, U32_Set *set);
bool swapchain_adequate(VkPhysicalDevice phys_device);
VkSurfaceFormatKHR choose_swapchain_fmt();
VkPresentModeKHR choose_present_mode();
VkExtent2D choose_swp_extent();
bool is_device_suitable(VkPhysicalDevice phys_device);
bool pick_phys_device();
void cleanup_swapchain();
bool find_mem_type_idx(uint32_t type, VkMemoryPropertyFlags properties, uint32_t *idx);
void frame_buff_resized(GLFWwindow* window, int width, int height);

#ifndef CVR_CAMERA
typedef struct {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    float fovy;
    int projection;
} Camera;
#endif

void cvr_set_proj(Camera camera);
void cvr_set_view(Camera camera);

#ifndef CVR_COLOR
typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;
#endif

void vk_begin_render_pass(Color color);

#endif // VK_CTX_H_

/***********************************************************************************
*
*   Vulkan Context Implementation
*
************************************************************************************/

#ifdef VK_CTX_IMPLEMENTATION

#include "ext/raylib-5.0/raymath.h"
#include <vulkan/vulkan.h>
#include <time.h>

#define Z_NEAR 0.01
#define Z_FAR 1000.0

Vk_Context ctx = {0};

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer gfx_buff;
    VkCommandBuffer compute_buff;
    VkSemaphore img_avail_sem;
    VkSemaphore render_fin_sem;
    VkSemaphore compute_fin_sem;
    VkFence fence;
    VkFence compute_fence;
} Vk_Cmd_Man;
static Vk_Cmd_Man cmd_man = {0};

#define MAX_CMD_BUFF_STACK 5
VkCommandBuffer cmd_buff_stack[MAX_CMD_BUFF_STACK];
size_t cmd_buff_tos = 0;

bool cmd_man_init();
bool cmd_buff_create();
bool cmd_syncs_create();
bool cmd_pool_create();
void cmd_man_destroy();

/* Allocates and begins a temporary command buffer. Easy-to-use, not super efficent. */
bool cmd_quick_begin(VkCommandBuffer *tmp_cmd_buff);

/* Ends and frees a temporary command buffer. Easy-to-use, not super efficent. */
bool cmd_quick_end(VkCommandBuffer *tmp_cmd_buff);

/* Manage vulkan extensions*/
typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Strings;

typedef struct {
    Strings validation_layers;
    Strings device_exts;
    Strings inst_exts;
    int inited;
} Ext_Manager;

void init_ext_managner();
void destroy_ext_manager();
bool inst_exts_satisfied();
bool chk_validation_support();
bool device_exts_supported(VkPhysicalDevice phys_device);

bool vk_img_init(Vk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
bool vk_img_copy(VkImage dst_img, VkBuffer src_buff, VkExtent2D extent);
bool vk_load_texture(void *data, size_t width, size_t height, VkFormat fmt, size_t *id, Descriptor_Type ds_type);
bool vk_unload_texture(size_t id, Descriptor_Type ds_type);

/* Add various static extensions & validation layers here */
static const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
static const char *device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
Ext_Manager ext_manager = {0};

/* Vertex attributes */
VkVertexInputBindingDescription get_binding_desc(Pipeline_Type pipeline_type);
typedef struct {
    VkVertexInputAttributeDescription *items;
    size_t count;
    size_t capacity;
} VtxAttrDescs;

void get_attr_descs(VtxAttrDescs *attr_descs, Pipeline_Type pipeline_type);

static uint32_t img_idx = 0;

bool vk_init()
{
    bool result = true;

    init_ext_managner();

    cvr_chk(vk_instance_init(), "failed to create instance");
#ifdef ENABLE_VALIDATION
    cvr_chk(setup_debug_msgr(), "failed to setup debug messenger");
#endif
    cvr_chk(vk_surface_init(), "failed to create vulkan surface");
    cvr_chk(pick_phys_device(), "failed to find suitable GPU");
    cvr_chk(vk_device_init(), "failed to create logical device");
    cvr_chk(vk_swapchain_init(), "failed to create swapchain");
    cvr_chk(vk_img_views_init(), "failed to create image views");
    cvr_chk(vk_render_pass_init(), "failed to create render pass");
    cvr_chk(vk_depth_init(), "failed to init depth resources");
    cvr_chk(vk_frame_buffs_init(), "failed to create frame buffers");
    cvr_chk(cmd_man_init(), "failed to create vulkan command manager");

defer:
    return result;
}

bool vk_destroy()
{
    cmd_man_destroy(&cmd_man);
    cleanup_swapchain();
    vkDeviceWaitIdle(ctx.device);
    for (size_t i = 0; i < DS_TYPE_COUNT; i++) {
        Vk_Texture_Set texture_set = ctx.texture_sets[i];
        vkDestroyDescriptorPool(ctx.device, texture_set.descriptor_pool, NULL);
    }
    for (size_t i = 0; i < DS_TYPE_COUNT; i++) {
        Vk_Texture_Set texture_set = ctx.texture_sets[i];
        vkDestroyDescriptorSetLayout(ctx.device, texture_set.set_layout, NULL);
    }
    for (size_t i = 0; i < PIPELINE_COUNT; i++) {
        vkDestroyPipeline(ctx.device, ctx.pipelines[i], NULL);
        vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layouts[i], NULL);
    }
    for (size_t i = 0; i < DS_TYPE_COUNT; i++) {
        Vk_Pipeline_Set pipeline_set = ctx.compute_pl_sets[i];
        for (size_t j = 0; j < pipeline_set.count; j++) {
            vkDestroyPipeline(ctx.device, pipeline_set.items[j], NULL);
        }
    }
    for (size_t i = 0; i < DS_TYPE_COUNT; i++) {
        Vk_Pipeline_Layout_Set layout_set = ctx.compute_pl_layout_sets[i];
        for (size_t j = 0; j < layout_set.count; j++)
            vkDestroyPipelineLayout(ctx.device, layout_set.items[j], NULL);
    }

    vkDestroyRenderPass(ctx.device, ctx.render_pass, NULL);
    vkDestroyDevice(ctx.device, NULL);
#ifdef ENABLE_VALIDATION
    load_pfn(vkDestroyDebugUtilsMessengerEXT);
    if (vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debug_msgr, NULL);
#endif
    vkDestroySurfaceKHR(ctx.instance, ctx.surface, NULL);
    vkDestroyInstance(ctx.instance, NULL);
    destroy_ext_manager();
    return true;
}

bool vk_instance_init()
{
    bool result = true;
#ifdef ENABLE_VALIDATION
    cvr_chk(chk_validation_support(), "validation requested, but not supported");
#endif

    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "C + Vulkan = Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_ci = {0};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;
    uint32_t glfw_ext_count = 0;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    for (size_t i = 0; i < glfw_ext_count; i++)
        nob_da_append(&ext_manager.inst_exts, glfw_exts[i]);
#ifdef ENABLE_VALIDATION
    instance_ci.enabledLayerCount = ext_manager.validation_layers.count;
    instance_ci.ppEnabledLayerNames = ext_manager.validation_layers.items;
    nob_da_append(&ext_manager.inst_exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
    populated_debug_msgr_ci(&debug_msgr_ci);
    instance_ci.pNext = &debug_msgr_ci;
#endif
    instance_ci.enabledExtensionCount = ext_manager.inst_exts.count;
    instance_ci.ppEnabledExtensionNames = ext_manager.inst_exts.items;

    cvr_chk(inst_exts_satisfied(), "unsatisfied instance extensions");
    vk_chk(vkCreateInstance(&instance_ci, NULL, &ctx.instance), "failed to create vulkan instance");

defer:
    return result;
}

typedef struct {
    VkDeviceQueueCreateInfo *items;
    size_t count;
    size_t capacity;
} Queue_Create_Infos;

bool vk_device_init()
{
    Queue_Fams queue_fams = find_queue_fams(ctx.phys_device);

    int queue_fam_idxs[] = {queue_fams.gfx_idx, queue_fams.present_idx};
    U32_Set unique_fams = {0};
    populate_set(queue_fam_idxs, NOB_ARRAY_LEN(queue_fam_idxs), &unique_fams);

    Queue_Create_Infos queue_cis = {0};
    float queuePriority = 1.0f;
    for (size_t i = 0; i < unique_fams.count; i++) {
        VkDeviceQueueCreateInfo queue_ci = {0};
        queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.queueFamilyIndex = unique_fams.items[i];
        queue_ci.queueCount = 1;
        queue_ci.pQueuePriorities = &queuePriority;
        nob_da_append(&queue_cis, queue_ci);
    }

    VkPhysicalDeviceFeatures features = {
        .samplerAnisotropy = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
        .shaderInt64 = VK_TRUE,
    };
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
        .shaderBufferInt64Atomics = VK_TRUE,
    };
    VkPhysicalDeviceFeatures2 all_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12,
        .features = features,
    };
    VkDeviceCreateInfo device_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &all_features,
        .pEnabledFeatures = NULL, // necessary when pNext is used
        .pQueueCreateInfos = queue_cis.items,
        .queueCreateInfoCount = queue_cis.count,
        .enabledExtensionCount = ext_manager.device_exts.count,
        .ppEnabledExtensionNames = ext_manager.device_exts.items,
    };
#ifdef ENABLE_VALIDATION
    device_ci.enabledLayerCount = ext_manager.validation_layers.count;
    device_ci.ppEnabledLayerNames = ext_manager.validation_layers.items;
#endif

    bool result = true;
    if (VK_SUCCEEDED(vkCreateDevice(ctx.phys_device, &device_ci, NULL, &ctx.device))) {
        vkGetDeviceQueue(ctx.device, queue_fams.gfx_idx, 0, &ctx.gfx_queue);
        vkGetDeviceQueue(ctx.device, queue_fams.present_idx, 0, &ctx.present_queue);
        vkGetDeviceQueue(ctx.device, queue_fams.compute_idx, 0, &ctx.compute_queue);
    } else {
        nob_return_defer(false);
    }

defer:
    nob_da_free(queue_cis);
    return result;
}

bool vk_surface_init()
{
    return VK_SUCCEEDED(glfwCreateWindowSurface(ctx.instance, ctx.window, NULL, &ctx.surface));
}

bool vk_swapchain_init()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.phys_device, ctx.surface, &capabilities);
    ctx.surface_fmt = choose_swapchain_fmt();
    uint32_t img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && img_count > capabilities.maxImageCount)
        img_count = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swapchain_ci = {0};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.surface = ctx.surface;
    swapchain_ci.minImageCount = img_count;
    swapchain_ci.imageFormat = ctx.surface_fmt.format;
    swapchain_ci.imageColorSpace = ctx.surface_fmt.colorSpace;
    swapchain_ci.imageExtent = ctx.extent = choose_swp_extent();
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    Queue_Fams queue_fams = find_queue_fams(ctx.phys_device);
    uint32_t queue_fam_idxs[] = {queue_fams.gfx_idx, queue_fams.present_idx};
    if (queue_fams.gfx_idx != queue_fams.present_idx) {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_ci.queueFamilyIndexCount = NOB_ARRAY_LEN(queue_fam_idxs);
        swapchain_ci.pQueueFamilyIndices = queue_fam_idxs;
    } else {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.presentMode = choose_present_mode();
    swapchain_ci.preTransform = capabilities.currentTransform;

    if (VK_SUCCEEDED(vkCreateSwapchainKHR(ctx.device, &swapchain_ci, NULL, &ctx.swapchain.handle))) {
        uint32_t img_count = 0;
        vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain.handle, &img_count, NULL);
        nob_da_resize(&ctx.swapchain.imgs, img_count);
        vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain.handle, &img_count, ctx.swapchain.imgs.items);
        return true;
    } else {
        nob_log(NOB_ERROR, "failed to create swapchain image");
        return false;
    }
}

bool vk_img_view_init(Vk_Image img, VkImageView *img_view)
{
    VkImageViewCreateInfo img_view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = img.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = img.aspect_mask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    VkResult res = vkCreateImageView(ctx.device, &img_view_ci, NULL, img_view);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create image view");
        return false;
    }

    return true;
}

bool vk_img_views_init()
{
    bool result = true;

    nob_da_resize(&ctx.swapchain.img_views, ctx.swapchain.imgs.count);
    for (size_t i = 0; i < ctx.swapchain.img_views.count; i++)  {
        Vk_Image img = {
            .handle = ctx.swapchain.imgs.items[i],
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = ctx.surface_fmt.format,
        };
        result = vk_img_view_init(img, &ctx.swapchain.img_views.items[i]);
        cvr_chk(result, "failed to initialize image view");
    }

defer:
    return result;
}

bool vk_basic_pl_init(Pipeline_Type pl_type)
{
    bool result = true;

    char *vert_shader_name;
    char *frag_shader_name;
    switch (pl_type) {
    case PIPELINE_TEXTURE:
        vert_shader_name = "./res/texture.vert.spv";
        frag_shader_name = "./res/texture.frag.spv";
        break;
    case PIPELINE_POINT_CLOUD_ADV:
    case PIPELINE_POINT_CLOUD:
        vert_shader_name = "./res/point-cloud.vert.spv";
        frag_shader_name = "./res/point-cloud.frag.spv";
        break;
    case PIPELINE_COMPUTE:
        vert_shader_name = "./res/particle.vert.spv";
        frag_shader_name = "./res/particle.frag.spv";
        break;
    case PIPELINE_DEFAULT:
    case PIPELINE_WIREFRAME:
    default:
        vert_shader_name = "./res/default.vert.spv";
        frag_shader_name = "./res/default.frag.spv";
        break;
    }

    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pName = "main",
        },
    };

    if (!vk_shader_mod_init(vert_shader_name, &stages[0].module)) nob_return_defer(false);
    if (!vk_shader_mod_init(frag_shader_name, &stages[1].module)) nob_return_defer(false);

    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {0};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    dynamic_state_ci.dynamicStateCount = NOB_ARRAY_LEN(dynamic_states);
    dynamic_state_ci.pDynamicStates = dynamic_states;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {0};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VtxAttrDescs vert_attrs = {0};
    get_attr_descs(&vert_attrs, pl_type);
    VkVertexInputBindingDescription binding_desc = get_binding_desc(pl_type);
    vertex_input_ci.vertexBindingDescriptionCount = 1;
    vertex_input_ci.pVertexBindingDescriptions = &binding_desc;
    vertex_input_ci.vertexAttributeDescriptionCount = vert_attrs.count;
    vertex_input_ci.pVertexAttributeDescriptions = vert_attrs.items;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {0};
    input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    if (pl_type == PIPELINE_POINT_CLOUD ||
        pl_type == PIPELINE_POINT_CLOUD_ADV ||
        pl_type == PIPELINE_COMPUTE)
        input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    else
        input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {
        .width    = (float) ctx.extent.width,
        .height   = (float) ctx.extent.height,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {.extent = ctx.extent,};
    VkPipelineViewportStateCreateInfo viewport_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {0};
    rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_ci.polygonMode = (pl_type == PIPELINE_WIREFRAME) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer_ci.lineWidth = 1.0f;
    rasterizer_ci.cullMode = VK_CULL_MODE_NONE;
    // rasterizer_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling_ci = {0};
    multisampling_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend = {
        .colorWriteMask = 0xf, // rgba
        .blendEnable = VK_TRUE,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend,
        .logicOp = VK_LOGIC_OP_COPY,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_ci = {0};
    pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    Descriptor_Set_Layouts set_layouts = {0};
    if (pl_type == PIPELINE_TEXTURE) {
        nob_da_append(&set_layouts, ctx.ubos[DS_TYPE_TEX].set_layout);
        nob_da_append(&set_layouts, ctx.texture_sets[DS_TYPE_TEX].set_layout);
    } else if (pl_type == PIPELINE_POINT_CLOUD_ADV) {
        nob_da_append(&set_layouts, ctx.ubos[DS_TYPE_ADV_POINT_CLOUD].set_layout);
        nob_da_append(&set_layouts, ctx.texture_sets[DS_TYPE_ADV_POINT_CLOUD].set_layout);
    }
    pipeline_layout_ci.pSetLayouts = set_layouts.items;
    pipeline_layout_ci.setLayoutCount = set_layouts.count;
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(float16),
    };
    pipeline_layout_ci.pPushConstantRanges = &pk_range;
    pipeline_layout_ci.pushConstantRangeCount = 1;
    VkResult res = vkCreatePipelineLayout(
        ctx.device,
        &pipeline_layout_ci,
        NULL,
        &ctx.pipeline_layouts[pl_type]
    );
    vk_chk(res, "failed to create pipeline layout");
    VkPipelineDepthStencilStateCreateInfo depth_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .maxDepthBounds = 1.0f,
    };

    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = NOB_ARRAY_LEN(stages),
        .pStages = stages,
        .pVertexInputState = &vertex_input_ci,
        .pInputAssemblyState = &input_assembly_ci,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterizer_ci,
        .pMultisampleState = &multisampling_ci,
        .pColorBlendState = &color_blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .pDepthStencilState = &depth_ci,
        .layout = ctx.pipeline_layouts[pl_type],
        .renderPass = ctx.render_pass,
        .subpass = 0,
    };

    res = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &ctx.pipelines[pl_type]);
    vk_chk(res, "failed to create pipeline");

defer:
    vkDestroyShaderModule(ctx.device, stages[0].module, NULL);
    vkDestroyShaderModule(ctx.device, stages[1].module, NULL);
    nob_da_free(vert_attrs);
    nob_da_free(set_layouts);
    return result;
}

bool vk_basic_pl_init2(VkPipelineLayout pl_layout, VkPipeline *pl)
{
    bool result = true;

    /* setup shader stages */
    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pName = "main",
        },
    };
    if (!vk_shader_mod_init("./res/default.vert.spv", &stages[0].module)) nob_return_defer(false);
    if (!vk_shader_mod_init("./res/default.frag.spv", &stages[1].module)) nob_return_defer(false);

    /* populate fields for graphics pipeline create info */
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = NOB_ARRAY_LEN(dynamic_states),
        .pDynamicStates = dynamic_states,
    };
    VkPipelineVertexInputStateCreateInfo empty_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkPipelineViewportStateCreateInfo viewport_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_FRONT_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    };
    VkPipelineMultisampleStateCreateInfo multisampling_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState color_blend = {
        .colorWriteMask = 0xf, // rgba
        .blendEnable = VK_FALSE,
    };
    VkPipelineColorBlendStateCreateInfo color_blend_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend,
    };
    VkPipelineDepthStencilStateCreateInfo depth_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .maxDepthBounds = 1.0f,
    };
    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = NOB_ARRAY_LEN(stages),
        .pStages = stages,
        .pVertexInputState = &empty_input_state,
        .pInputAssemblyState = &input_assembly_ci,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterizer_ci,
        .pMultisampleState = &multisampling_ci,
        .pColorBlendState = &color_blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .pDepthStencilState = &depth_ci,
        .layout = pl_layout,
        .renderPass = ctx.render_pass,
        .subpass = 0,
    };

    VkResult res = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, pl);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create basic pipline ");
        nob_return_defer(false);
    }

defer:
    vkDestroyShaderModule(ctx.device, stages[0].module, NULL);
    vkDestroyShaderModule(ctx.device, stages[1].module, NULL);
    return result;
}

bool vk_compute_pl_init(Descriptor_Type ds_type)
{
    bool result = true;

    VkPipelineShaderStageCreateInfo shader_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .pName = "main",
    };

    switch (ds_type) {
    case DS_TYPE_COMPUTE:
        if (!vk_shader_mod_init("./res/default.comp.spv", &shader_ci.module))
            nob_return_defer(false);
        break;
    case DS_TYPE_COMPUTE_RASTERIZER:
        if (!vk_shader_mod_init("./res/render.comp.spv", &shader_ci.module))
            nob_return_defer(false);
        break;
    default:
        nob_log(NOB_ERROR, "descriptor type %d not supported for compute pipeline", ds_type);
        nob_return_defer(false);
    }

    if (!ctx.ubos[ds_type].set_layout || !ctx.ssbo_sets[ds_type].set_layout) {
        nob_log(NOB_ERROR, "set layouts for compute are null");
        nob_return_defer(false);
    }

    Descriptor_Set_Layouts set_layouts = {0};
    nob_da_append(&set_layouts, ctx.ubos[ds_type].set_layout);
    nob_da_append(&set_layouts, ctx.ssbo_sets[ds_type].set_layout);

    VkPipelineLayoutCreateInfo pipeline_layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = set_layouts.count,
        .pSetLayouts = set_layouts.items,
    };
    VkPipelineLayout pl_layout;
    VkResult res = vkCreatePipelineLayout(
        ctx.device,
        &pipeline_layout_ci,
        NULL,
        &pl_layout
    );
    nob_da_append(&ctx.compute_pl_layout_sets[ds_type], pl_layout);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create layout for compute pipeline");
        nob_return_defer(false);
    }

    VkComputePipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = pl_layout,
        .stage = shader_ci,
    };

    VkPipeline pipeline;

    res = vkCreateComputePipelines(
        ctx.device,
        VK_NULL_HANDLE,
        1,
        &pipeline_ci,
        NULL,
        &pipeline
    );
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create compute pipeline");
        nob_return_defer(false);
    }
    nob_da_append(&ctx.compute_pl_sets[ds_type], pipeline);

    VkPipelineShaderStageCreateInfo resolve_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .pName = "main",
    };
    if (ds_type == DS_TYPE_COMPUTE_RASTERIZER) {
        if (!vk_shader_mod_init("./res/resolve.comp.spv", &resolve_ci.module))
            nob_return_defer(false);

        /* add the storage image to the set layout */
        if (!ctx.texture_sets[ds_type].count) {
            nob_log(NOB_ERROR, "no texture sets available for compute rasterizer");
            nob_return_defer(false);
        }
        nob_da_append(&set_layouts, ctx.texture_sets[ds_type].set_layout);
        VkPipelineLayoutCreateInfo pipeline_layout_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = set_layouts.count,
            .pSetLayouts = set_layouts.items,
        };

        VkPipelineLayout comp_2_pl_layout;
        VkResult res = vkCreatePipelineLayout(
            ctx.device,
            &pipeline_layout_ci,
            NULL,
            &comp_2_pl_layout
        );
        nob_da_append(&ctx.compute_pl_layout_sets[ds_type], comp_2_pl_layout);

        VkPipeline comp_2_pl;
        VkComputePipelineCreateInfo pipeline_ci = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .layout = comp_2_pl_layout,
            .stage = resolve_ci,
        };
        res = vkCreateComputePipelines(
            ctx.device,
            VK_NULL_HANDLE,
            1,
            &pipeline_ci,
            NULL,
            &comp_2_pl
        );
        if (!VK_SUCCEEDED(res)) {
            nob_log(NOB_ERROR, "failed to create compute pipeline");
            nob_return_defer(false);
        }
        nob_da_append(&ctx.compute_pl_sets[ds_type], comp_2_pl);
    }

defer:
    vkDestroyShaderModule(ctx.device, shader_ci.module, NULL);
    if (ds_type == DS_TYPE_COMPUTE_RASTERIZER)
        vkDestroyShaderModule(ctx.device, resolve_ci.module, NULL);
    return result;
}

bool vk_pl_layout_init(VkDescriptorSetLayout layout, VkPipelineLayout *pl_layout)
{
    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &layout,
    };
    VkResult res = vkCreatePipelineLayout(ctx.device, &ci, NULL, pl_layout);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create pipeline layout");
        return false;
    }

    return true;
}

bool vk_compute_pl_init2(const char *shader_name, VkPipelineLayout pl_layout, VkPipeline *pipeline)
{
    bool result = true;

    VkPipelineShaderStageCreateInfo shader_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .pName = "main",
    };
    if (!vk_shader_mod_init(shader_name, &shader_ci.module)) nob_return_defer(false);

    VkComputePipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = pl_layout,
        .stage = shader_ci,
    };
    VkResult res = vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, pipeline);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create compute pipeline");
        nob_return_defer(false);
    }

defer:
    vkDestroyShaderModule(ctx.device, shader_ci.module, NULL);
    return result;
}

void cleanup_pipeline(VkPipeline pipeline, VkPipelineLayout pl_layout)
{
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pl_layout, NULL);
}

bool vk_shader_mod_init(const char *file_name, VkShaderModule *module)
{
    bool result = true;
    Nob_String_Builder sb = {};
    char *err_msg = nob_temp_sprintf("failed to read entire file %s", file_name);
    cvr_chk(nob_read_entire_file(file_name, &sb), err_msg);

    VkShaderModuleCreateInfo module_ci = {0};
    module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_ci.codeSize = sb.count;
    module_ci.pCode = (const uint32_t *)sb.items;
    err_msg = nob_temp_sprintf("failed to create shader module from %s", file_name);
    vk_chk(vkCreateShaderModule(ctx.device, &module_ci, NULL, module), err_msg);

defer:
    nob_sb_free(sb);
    return result;
}

bool vk_render_pass_init()
{
    VkAttachmentDescription color = {
        .format = ctx.surface_fmt.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription depth = {
        .format = VK_FORMAT_D32_SFLOAT, // TODO: check supported formats
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription gfx_subpass = {
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };

    VkAttachmentDescription attachments[] = {color, depth};
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = NOB_ARRAY_LEN(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &gfx_subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    return VK_SUCCEEDED(vkCreateRenderPass(ctx.device, &render_pass_ci, NULL, &ctx.render_pass));
}

bool vk_frame_buffs_init()
{
    nob_da_resize(&ctx.swapchain.buffs, ctx.swapchain.img_views.count);
    for (size_t i = 0; i < ctx.swapchain.img_views.count; i++) {
        VkImageView attachments[] = {ctx.swapchain.img_views.items[i], ctx.depth_img_view};

        VkFramebufferCreateInfo frame_buff_ci = {0};
        frame_buff_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buff_ci.renderPass = ctx.render_pass;
        frame_buff_ci.attachmentCount = NOB_ARRAY_LEN(attachments);
        frame_buff_ci.pAttachments = attachments;
        frame_buff_ci.width =  ctx.extent.width;
        frame_buff_ci.height = ctx.extent.height;
        frame_buff_ci.layers = 1;
        if (!VK_SUCCEEDED(vkCreateFramebuffer(ctx.device, &frame_buff_ci, NULL, &ctx.swapchain.buffs.items[i])))
            return false;
    }

    return true;
}

void vk_begin_render_pass(Color color)
{
    VkRenderPassBeginInfo begin_rp = {0};
    begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_rp.renderPass = ctx.render_pass;
    begin_rp.framebuffer = ctx.swapchain.buffs.items[img_idx];
    begin_rp.renderArea.extent = ctx.extent;
    VkClearValue clear_color = {
        .color = {
            {
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f,
            }
        }
    };
    VkClearValue clear_depth = {
        .depthStencil = {
            .depth = 1.0f,
            .stencil = 0,
        }
    };
    VkClearValue clear_values[] = {clear_color, clear_depth};
    begin_rp.clearValueCount = NOB_ARRAY_LEN(clear_values);
    begin_rp.pClearValues = clear_values;
    vkCmdBeginRenderPass(cmd_man.gfx_buff, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
}

bool vk_draw(Pipeline_Type pipeline_type, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix mvp)
{
    bool result = true;

    if (pipeline_type == PIPELINE_POINT_CLOUD || pipeline_type == PIPELINE_POINT_CLOUD_ADV) {
        nob_log(NOB_ERROR, "point cloud pipelines should use vk_draw_points");
        nob_return_defer(false);
    }

    VkCommandBuffer cmd_buffer = cmd_man.gfx_buff;
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipelines[pipeline_type]);
    VkViewport viewport = {0};
    viewport.width = (float)ctx.extent.width;
    viewport.height =(float)ctx.extent.height;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = ctx.extent;
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vtx_buff.handle, offsets);
    vkCmdBindIndexBuffer(cmd_buffer, idx_buff.handle, 0, VK_INDEX_TYPE_UINT16);

    float16 mat = MatrixToFloatV(mvp);
    vkCmdPushConstants(
        cmd_buffer,
        ctx.pipeline_layouts[pipeline_type],
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(float16),
        &mat
    );

    vkCmdDrawIndexed(cmd_buffer, idx_buff.count, 1, 0, 0, 0);

defer:
    return result;
}

bool vk_draw_sst()
{
    bool result = true;

    if (!ctx.pipelines[PIPELINE_SST] || ctx.texture_sets[DS_TYPE_COMPUTE_RASTERIZER].descriptor_set) {
        nob_log(NOB_ERROR, "pipeline or texture set not setup");
        nob_return_defer(false);
    }

    VkCommandBuffer cmd_buffer = cmd_man.gfx_buff;
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipelines[PIPELINE_SST]);
    VkViewport viewport = {
        .width    = (float)ctx.extent.width,
        .height   = (float)ctx.extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = { .extent = ctx.extent, };
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(
        cmd_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ctx.pipeline_layouts[PIPELINE_POINT_CLOUD_ADV], 0, 1,
        &ctx.texture_sets[DS_TYPE_COMPUTE_RASTERIZER].descriptor_set, 0, NULL
    );
    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);

defer:
    return result;
}

void vk_compute(Descriptor_Type ds_type)
{
    Vk_Pipeline_Set pipeline_set = ctx.compute_pl_sets[ds_type];
    vkCmdBindPipeline(
        cmd_man.compute_buff,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline_set.items[0]
    );

    vkCmdBindDescriptorSets(
        cmd_man.compute_buff,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        ctx.compute_pl_layout_sets[ds_type].items[0], 0, 1,
        &ctx.ubos[ds_type].descriptor_set,
        0, NULL
    );

    assert(ctx.ssbo_sets[ds_type].count && "no compute buffers");

    vkCmdBindDescriptorSets(
        cmd_man.compute_buff,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        ctx.compute_pl_layout_sets[ds_type].items[0], 1, 1,
        &ctx.ssbo_sets[ds_type].descriptor_set,
        0, NULL
    );

    float k_size = (ds_type == DS_TYPE_COMPUTE_RASTERIZER) ? 128 : 256;
    uint32_t group_x = ceil(ctx.ssbo_sets[ds_type].items[0].count / k_size);

    vkCmdDispatch(cmd_man.compute_buff, group_x, 1, 1);

    /* resolve pipeline */
    if (ds_type == DS_TYPE_COMPUTE_RASTERIZER) {

        /* barrier */
        VkMemoryBarrier2KHR barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR
        };

        VkDependencyInfo dependency = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier,
        };

        vkCmdPipelineBarrier2(cmd_man.compute_buff, &dependency);

        /* bind to resolve pipeline */
        vkCmdBindPipeline(
            cmd_man.compute_buff,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline_set.items[1]
        );

        /* resolve descriptor sets */
        vkCmdBindDescriptorSets(
            cmd_man.compute_buff,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            ctx.compute_pl_layout_sets[ds_type].items[1], 0, 1,
            &ctx.ubos[ds_type].descriptor_set,
            0, NULL
        );
        vkCmdBindDescriptorSets(
            cmd_man.compute_buff,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            ctx.compute_pl_layout_sets[ds_type].items[1], 1, 1,
            &ctx.ssbo_sets[ds_type].descriptor_set,
            0, NULL
        );
        vkCmdBindDescriptorSets(
            cmd_man.compute_buff,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            ctx.compute_pl_layout_sets[ds_type].items[1], 2, 1,
            &ctx.texture_sets[ds_type].descriptor_set,
            0, NULL
        );

        /* dispatch */
        vkCmdDispatch(cmd_man.compute_buff, 1600 / 16, 900 / 16, 1);
    }
}

void vk_compute2(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds, size_t x, size_t y, size_t z)
{
    vkCmdBindPipeline(cmd_man.compute_buff, VK_PIPELINE_BIND_POINT_COMPUTE, pl);
    vkCmdBindDescriptorSets(cmd_man.compute_buff, VK_PIPELINE_BIND_POINT_COMPUTE, pl_layout, 0, 1, &ds, 0, NULL);
    vkCmdDispatch(cmd_man.compute_buff, x, y, z);
}

void vk_gfx(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds)
{
    vkCmdBindPipeline(cmd_man.gfx_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    vkCmdBindDescriptorSets(cmd_man.gfx_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl_layout, 0, 1, &ds, 0, NULL);

    VkViewport viewport = {
        .width = (float)ctx.extent.width,
        .height =(float)ctx.extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd_man.gfx_buff, 0, 1, &viewport);
    VkRect2D scissor = { .extent = ctx.extent, };
    vkCmdSetScissor(cmd_man.gfx_buff, 0, 1, &scissor);
    vkCmdDraw(cmd_man.gfx_buff, 3, 1, 0, 0);
}

bool vk_rec_compute()
{
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, };
    VkResult res = vkBeginCommandBuffer(cmd_man.compute_buff, &begin_info);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to begin compute command buffer");
        return false;
    }

    return true;
}

bool vk_end_rec_compute()
{
    VkResult res = vkEndCommandBuffer(cmd_man.compute_buff);                   
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to end compute pass");
        return false;
    }

    return true;
}

bool vk_submit_compute()
{
    /* first wait for compute fence */
    VkResult res = vkWaitForFences(
        ctx.device, 1, &cmd_man.compute_fence, VK_TRUE, UINT64_MAX
    );
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed waiting for compute fence");
        return false;
    }
    res = vkResetFences(ctx.device, 1, &cmd_man.compute_fence);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed resetting compute fence");
        return false;
    }
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_man.compute_buff,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &cmd_man.compute_fin_sem,
    };

    res = vkQueueSubmit(ctx.compute_queue, 1, &submit, cmd_man.compute_fence);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to submit compute queue");
        return false;
    }

    return true;
}

void vk_compute_pl_barrier()
{
    VkMemoryBarrier2KHR barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR
    };

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmd_man.compute_buff, &dependency);
}

bool vk_draw_points(Vk_Buffer vtx_buff, Matrix mvp, Example example)
{
    bool result = true;

    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    if (example == EXAMPLE_ADV_POINT_CLOUD) {
        pipeline = ctx.pipelines[PIPELINE_POINT_CLOUD_ADV];
        pipeline_layout = ctx.pipeline_layouts[PIPELINE_POINT_CLOUD_ADV];
    } else if (example == EXAMPLE_COMPUTE) {
        pipeline = ctx.pipelines[PIPELINE_COMPUTE];
        pipeline_layout = ctx.pipeline_layouts[PIPELINE_COMPUTE];
    } else {
        pipeline = ctx.pipelines[PIPELINE_POINT_CLOUD];
        pipeline_layout = ctx.pipeline_layouts[PIPELINE_POINT_CLOUD];
    }

    VkCommandBuffer cmd_buffer = cmd_man.gfx_buff;
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkViewport viewport = {0};
    viewport.width = (float)ctx.extent.width;
    viewport.height =(float)ctx.extent.height;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = ctx.extent;
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vtx_buff.handle, offsets);

    if (example == EXAMPLE_ADV_POINT_CLOUD) {
        VkDescriptorSet *descriptor_set = &ctx.ubos[DS_TYPE_ADV_POINT_CLOUD].descriptor_set;
        vkCmdBindDescriptorSets(
            cmd_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx.pipeline_layouts[PIPELINE_POINT_CLOUD_ADV], 0, 1, descriptor_set, 0, NULL
        );

        vkCmdBindDescriptorSets(
            cmd_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx.pipeline_layouts[PIPELINE_POINT_CLOUD_ADV], 1, 1,
            &ctx.texture_sets[DS_TYPE_ADV_POINT_CLOUD].descriptor_set, 0, NULL
        );
    }

    float16 mat = MatrixToFloatV(mvp);
    vkCmdPushConstants(
        cmd_buffer,
        pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(float16),
        &mat
    );

    vkCmdDraw(cmd_buffer, vtx_buff.count, 1, 0, 0);

    return result;
}

bool vk_draw_texture(size_t id, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix mvp)
{
    bool result = true;
    (void) id; (void)vtx_buff; (void)idx_buff, (void)mvp;

    VkCommandBuffer cmd_buffer = cmd_man.gfx_buff;

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipelines[PIPELINE_TEXTURE]);
    VkViewport viewport = {0};
    viewport.width = (float)ctx.extent.width;
    viewport.height =(float)ctx.extent.height;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = ctx.extent;
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vtx_buff.handle, offsets);
    vkCmdBindIndexBuffer(cmd_buffer, idx_buff.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(
        cmd_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ctx.pipeline_layouts[PIPELINE_TEXTURE], 0, 1,
        &ctx.ubos[DS_TYPE_TEX].descriptor_set, 0, NULL
    );

    Vk_Texture_Set texture_set = ctx.texture_sets[DS_TYPE_TEX];
    if (id < texture_set.count && texture_set.items[id].active) {
        vkCmdBindDescriptorSets(
            cmd_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx.pipeline_layouts[PIPELINE_TEXTURE], 1, 1,
            &texture_set.items[id].descriptor_set, 0, NULL
        );
    }

    float16 mat = MatrixToFloatV(mvp);
    vkCmdPushConstants(
        cmd_buffer,
        ctx.pipeline_layouts[PIPELINE_TEXTURE],
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(float16),
        &mat
    );

    vkCmdDrawIndexed(cmd_buffer, idx_buff.count, 1, 0, 0, 0);

    return result;
}

bool vk_begin_drawing()
{
    VkResult res = vkWaitForFences(ctx.device, 1, &cmd_man.fence, VK_TRUE, UINT64_MAX);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to wait for fences");
        return false;
    }

    res = vkAcquireNextImageKHR(
        ctx.device, ctx.swapchain.handle, UINT64_MAX,
        cmd_man.img_avail_sem, VK_NULL_HANDLE, &img_idx
    );
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!vk_swapchain_init()) return false;
    } else if (!VK_SUCCEEDED(res) && res != VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_ERROR, "failed to acquire swapchain image");
        return false;
    } else if (res == VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_WARNING, "suboptimal swapchain image");
    }

    res = vkResetFences(ctx.device, 1, &cmd_man.fence);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to reset fences");
        return false;
    }
    res = vkResetCommandBuffer(cmd_man.gfx_buff, 0);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR,"failed to reset cmd buffer");
        return false;
    }

    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, };
    res = vkBeginCommandBuffer(cmd_man.gfx_buff, &begin_info);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR,"failed to begin command buffer");
        return false;
    }

    return true;
}

bool vk_begin_compute()
{
    bool result = true;

    VkResult res = vkWaitForFences(
        ctx.device, 1, &cmd_man.compute_fence, VK_TRUE, UINT64_MAX
    );
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed waiting for compute fence");
        nob_return_defer(false);
    }
    res = vkResetFences(ctx.device, 1, &cmd_man.compute_fence);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed resetting compute fence");
        nob_return_defer(false);
    }
    res = vkResetCommandBuffer(cmd_man.compute_buff, 0);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed resetting compute command buffer");
        nob_return_defer(false);
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    res = vkBeginCommandBuffer(cmd_man.compute_buff, &begin_info);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to begin compute command buffer");
        nob_return_defer(false);
    }

defer:
    return result;
}

bool vk_end_compute()
{
    bool result = true;

    VkResult res = vkEndCommandBuffer(cmd_man.compute_buff);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to end compute pass");
        nob_return_defer(false);
    }

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_man.compute_buff,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &cmd_man.compute_fin_sem,
    };

    res = vkQueueSubmit(ctx.compute_queue, 1, &submit, cmd_man.compute_fence);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to submit compute queue");
        nob_return_defer(false);
    }

defer:
    return result;
}

bool vk_end_drawing()
{
    vkCmdEndRenderPass(cmd_man.gfx_buff);
    VkResult res = vkEndCommandBuffer(cmd_man.gfx_buff);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to record command buffer");
        return false;
    }

    VkSemaphore wait_sems[] = {cmd_man.compute_fin_sem, cmd_man.img_avail_sem};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_man.gfx_buff,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &cmd_man.render_fin_sem,
    };
    // if (ctx.pipelines[PIPELINE_COMPUTE]) {
    if (true) { // TODO: this breaks all other examples
        submit.waitSemaphoreCount = 2;
        submit.pWaitSemaphores = wait_sems;
        submit.pWaitDstStageMask = wait_stages;
    } else {
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &wait_sems[1];
        submit.pWaitDstStageMask = &wait_stages[1];
    }

    res = vkQueueSubmit(ctx.gfx_queue, 1, &submit, cmd_man.fence);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to submit to graphics queue");
        return false;
    }

    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &cmd_man.render_fin_sem,
        .swapchainCount = 1,
        .pSwapchains = &ctx.swapchain.handle,
        .pImageIndices = &img_idx,
    };
    res = vkQueuePresentKHR(ctx.present_queue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || ctx.swapchain.buff_resized) {
        ctx.swapchain.buff_resized = false;
        if (!vk_recreate_swapchain()) return false;
    } else if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to present queue");
        return false;
    }

    return true;
}

bool vk_recreate_swapchain()
{
    bool result = true;

    int width = 0, height = 0;
    glfwGetFramebufferSize(ctx.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(ctx.window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx.device);

    cleanup_swapchain();

    cvr_chk(vk_swapchain_init(), "failed to recreate swapchain");
    cvr_chk(vk_img_views_init(), "failed to recreate image views");
    cvr_chk(vk_depth_init(), "failed to init depth resources");
    cvr_chk(vk_frame_buffs_init(), "failed to recreate frame buffers");

defer:
    return result;
}

bool vk_depth_init()
{
    bool result = true;

    ctx.depth_img.extent = ctx.extent;
    ctx.depth_img.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
    ctx.depth_img.format = VK_FORMAT_D32_SFLOAT; // TODO: check supported formats
    result = vk_img_init(
        &ctx.depth_img,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to initialize depth image");
    result = vk_img_view_init(ctx.depth_img, &ctx.depth_img_view);

defer:
    return result;
}

bool vk_ubo_init(UBO ubo, Descriptor_Type type)
{
    bool result = true;

    if (ctx.ubos[type].buff.handle) {
        nob_log(NOB_ERROR, "attempting to leak memory for ubo %d", type);
        nob_return_defer(false);
    } else if (ubo.buff.size == 0) {
        nob_log(NOB_ERROR, "must specify ubo size");
        nob_return_defer(false);
    } else if (ubo.data == NULL) {
        nob_log(NOB_ERROR, "ubo does not point to any data");
        nob_return_defer(false);
    } else {
        ctx.ubos[type] = ubo;
    }

    Vk_Buffer *buff = &ctx.ubos[type].buff;
    result = vk_buff_init(
        buff,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (!result) nob_return_defer(false);
    vkMapMemory(ctx.device, buff->mem, 0, buff->size, 0, &buff->mapped);

defer:
    return result;
}

bool vk_ubo_init2(UBO *ubo)
{
    Vk_Buffer *buff = &ubo->buff;
    bool result = vk_buff_init(
        buff,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (result) vkMapMemory(ctx.device, buff->mem, 0, buff->size, 0, &buff->mapped);
    else return false;

    return true;
}

bool vk_ubo_descriptor_set_layout_init(VkShaderStageFlags flags, Descriptor_Type ds_type)
{
    bool result = true;

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = flags,
    };

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    VkDescriptorSetLayout *layout = &ctx.ubos[ds_type].set_layout;
    VkResult res = vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, layout);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor set layout for uniform buffer");
        nob_return_defer(false);
    }

defer:
    return result;
}

typedef struct {
    VkDescriptorPoolSize *items;
    size_t count;
    size_t capacity;
} Pool_Sizes;

bool vk_ubo_descriptor_pool_init(Descriptor_Type ds_type)
{
    bool result = true;

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = 1,
    };

    VkDescriptorPool *pool = &ctx.ubos[ds_type].descriptor_pool;
    VkResult res = vkCreateDescriptorPool(ctx.device, &pool_ci, NULL, pool);
    if(!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor pool");
        nob_return_defer(false);
    }

defer:
    return result;
}

typedef struct {
    VkWriteDescriptorSet *items;
    size_t count;
    size_t capacity;
} Descriptor_Writes;

bool vk_ubo_descriptor_set_init(Descriptor_Type ds_type)
{
    bool result = true;

    /* allocate uniform buffer descriptor sets */
    UBO *ubo = &ctx.ubos[ds_type];
    VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ubo->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ubo->set_layout,
    };
    VkResult res = vkAllocateDescriptorSets(ctx.device, &alloc, &ubo->descriptor_set);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to allocate ubo descriptor set");
        nob_return_defer(false);
    }

    /* update uniform buffer descriptor sets */
    VkDescriptorBufferInfo buff_info = {
        .buffer = ubo->buff.handle,
        .offset = 0,
        .range = ubo->buff.size,
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ubo->descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .pBufferInfo = &buff_info,
    };

    vkUpdateDescriptorSets(ctx.device, 1, &write, 0, NULL);

defer:
    return result;
}

bool vk_sampler_descriptor_set_layout_init(Descriptor_Type ds_type, VkShaderStageFlags flags)
{
    bool result = true;

    Descriptor_Set_Layout_Bindings bindings = {0};
    size_t tex_count = ctx.texture_sets[ds_type].count;
    if (!tex_count) {
        nob_log(NOB_ERROR, "no textures were loaded for descriptor type %d", ds_type);
        nob_return_defer(false);
    }

    VkDescriptorType vk_ds_type;
    vk_ds_type = (ds_type == DS_TYPE_COMPUTE_RASTERIZER) ?
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bool grouped_bindings = ds_type == DS_TYPE_ADV_POINT_CLOUD || ds_type == DS_TYPE_COMPUTE_RASTERIZER;
    for (size_t i = 0; i < tex_count; i++) {
        if (!grouped_bindings && i > 0)
            break;

        VkDescriptorSetLayoutBinding binding = {
            .binding = i,
            .descriptorType = vk_ds_type,
            .descriptorCount = 1,
            .stageFlags = flags,
        };
        nob_da_append(&bindings, binding);
    }

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pBindings = bindings.items,
        .bindingCount = bindings.count,
    };

    VkDescriptorSetLayout *layout = &ctx.texture_sets[ds_type].set_layout;
    if (!VK_SUCCEEDED(vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, layout))) {
        nob_log(NOB_ERROR, "failed to create descriptor set layout for texture");
        nob_return_defer(false);
    }

defer:
    nob_da_free(bindings);
    return result;
}

bool vk_sampler_descriptor_pool_init(Descriptor_Type ds_type)
{
    bool result = true;

    Vk_Texture_Set *texture_set = &ctx.texture_sets[ds_type];
    if (!texture_set->count) {
        nob_log(NOB_ERROR, "descriptor pool cannot be created for texture set count zero");
        nob_return_defer(false);
    }

    VkDescriptorType vk_ds_type;
    vk_ds_type = (ds_type == DS_TYPE_COMPUTE_RASTERIZER) ?
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    VkDescriptorPoolSize pool_size = {
        .type = vk_ds_type,
        .descriptorCount = texture_set->count,
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = texture_set->count,
    };

    VkDescriptorPool *pool = &texture_set->descriptor_pool;
    VkResult res = vkCreateDescriptorPool(ctx.device, &pool_ci, NULL, pool);
    if(!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor pool");
        nob_return_defer(false);
    }

defer:
    return result;
}

bool vk_sampler_descriptor_set_init(Descriptor_Type ds_type)
{
    bool result = true;

    /* allocate texture descriptor sets */
    Vk_Texture_Set *texture_set = &ctx.texture_sets[ds_type];
    VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = texture_set->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &texture_set->set_layout,
    };

    bool grouped_bindings = ds_type == DS_TYPE_ADV_POINT_CLOUD || ds_type == DS_TYPE_COMPUTE_RASTERIZER;
    if (grouped_bindings) {
        VkDescriptorSet *set = &texture_set->descriptor_set;
        VkResult res = vkAllocateDescriptorSets(ctx.device, &alloc, set);
        if (!VK_SUCCEEDED(res)) {
            nob_log(NOB_ERROR, "failed to allocate texture descriptor set");
            nob_return_defer(false);
        }
    } else {
        for (size_t i = 0; i < texture_set->count; i++) {
            VkDescriptorSet *set = &texture_set->items[i].descriptor_set;
            VkResult res = vkAllocateDescriptorSets(ctx.device, &alloc, set);
            if (!VK_SUCCEEDED(res)) {
                nob_log(NOB_ERROR, "failed to allocate texture descriptor set");
                nob_return_defer(false);
            }
        }
    }

    VkDescriptorType vk_ds_type;
    vk_ds_type = (ds_type == DS_TYPE_COMPUTE_RASTERIZER) ?
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    /* update texture descriptor sets */
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = vk_ds_type,
        .descriptorCount = 1,
    };

    VkImageLayout vk_img_layout;
    vk_img_layout = (ds_type == DS_TYPE_COMPUTE_RASTERIZER) ?
        VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    for (size_t tex = 0; tex < texture_set->count; tex++) {
        VkDescriptorImageInfo img_info = {
            .imageLayout = vk_img_layout,
            .imageView   = texture_set->items[tex].view,
            .sampler     = texture_set->items[tex].sampler,
        };
        write.pImageInfo = &img_info;
        if (grouped_bindings) {
            write.dstBinding = tex;
            write.dstSet = texture_set->descriptor_set;
        } else {
            write.dstSet = texture_set->items[tex].descriptor_set;
        }
        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, NULL);
    }

defer:
    return result;
}

bool vk_ssbo_descriptor_set_layout_init(Descriptor_Type ds_type)
{
    bool result = true;

    Descriptor_Set_Layout_Bindings bindings = {0};
    size_t ssbo_count = ctx.ssbo_sets[ds_type].count;
    if (!ssbo_count) {
        nob_log(NOB_ERROR, "no ssbos were loaded for descriptor type %d", ds_type);
        nob_return_defer(false);
    }

    bool grouped_bindings = ds_type == DS_TYPE_COMPUTE_RASTERIZER;
    for (size_t i = 0; i < ssbo_count; i++) {
        if (!grouped_bindings && i > 0)
            break;

        VkDescriptorSetLayoutBinding binding = {
            .binding = i,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };
        nob_da_append(&bindings, binding);
    }

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = bindings.count,
        .pBindings = bindings.items,
    };

    VkDescriptorSetLayout *layout = &ctx.ssbo_sets[ds_type].set_layout;
    VkResult res = vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, layout);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor set layout for shader storage buffer");
        nob_return_defer(false);
    }

defer:
    nob_da_free(bindings);
    return result;
}

bool vk_ssbo_descriptor_pool_init(Descriptor_Type ds_type)
{
    bool result = true;

    size_t ssbo_count = ctx.ssbo_sets[ds_type].count;
    if (!ssbo_count) {
        nob_log(NOB_ERROR, "no ssbos were loaded for descriptor type %d", ds_type);
        nob_return_defer(false);
    }

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = ssbo_count,
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = ssbo_count,
    };

    VkDescriptorPool *pool = &ctx.ssbo_sets[ds_type].descriptor_pool;
    VkResult res = vkCreateDescriptorPool(ctx.device, &pool_ci, NULL, pool);
    if(!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor pool for storage buffer");
        nob_return_defer(false);
    }

defer:
    return result;
}

bool vk_ssbo_descriptor_set_init(Descriptor_Type ds_type)
{
    bool result = true;

    /* allocate uniform buffer descriptor sets */
    SSBO_Set *ssbo = &ctx.ssbo_sets[ds_type];
    VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ssbo->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ssbo->set_layout,
    };

    bool grouped_bindings = ds_type == DS_TYPE_COMPUTE_RASTERIZER || DS_TYPE_COMPUTE;
    if (grouped_bindings) {
        if (!ssbo->count) {
            nob_log(NOB_ERROR, "failed to initialize compute descriptor set, no compute buffers");
            nob_return_defer(false);
        }
        VkResult res = vkAllocateDescriptorSets(ctx.device, &alloc, &ssbo->descriptor_set);
        if (!VK_SUCCEEDED(res)) {
            nob_log(NOB_ERROR, "failed to allocate ssbo descriptor set");
            nob_return_defer(false);
        }
    } else {
        nob_log(NOB_ERROR, "currently don't support ungrouped bindings for ssbo");
        nob_return_defer(false);
    }

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
    };

    for (size_t i = 0; i < ssbo->count; i++) {
        VkDescriptorBufferInfo buff_info = {
            .buffer = ssbo->items[i].handle,
            .range  = ssbo->items[i].size,
        };
        write.pBufferInfo = &buff_info;
        if (grouped_bindings) {
            write.dstSet = ssbo->descriptor_set;
            write.dstBinding = i;
        } else {
            nob_log(NOB_ERROR, "currently don't support ungrouped bindings for ssbo");
            nob_return_defer(false);
            // NOTE: if we did we would do 
            // write.dstSet = &ssbo->items[i].descriptor_set
            // write.dstBinding = 0;
        }
        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, NULL);
    }


defer:
    return result;
}

bool is_device_suitable(VkPhysicalDevice phys_device)
{
    bool result = true;
    Queue_Fams queue_fams = find_queue_fams(phys_device);
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(phys_device, &props);
    vkGetPhysicalDeviceFeatures(phys_device, &features);
    result &= (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
               props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
    result &= (features.geometryShader);
    cvr_chk(queue_fams.has_gfx && queue_fams.has_present, "requested indices not present");
    cvr_chk(device_exts_supported(phys_device), "device extensions not supported");
    cvr_chk(swapchain_adequate(phys_device), "swapchain was not adequate");

defer:
    return result;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data)
{
    (void)msg_type;
    (void)p_user_data;
    Nob_Log_Level log_lvl = translate_msg_severity(msg_severity);
    if (log_lvl < MIN_SEVERITY) return VK_FALSE;
    nob_log(log_lvl, "[Vulkan Validation] %s", p_callback_data->pMessage);
    return VK_FALSE;
}

void populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *debug_msgr_ci)
{
    debug_msgr_ci->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_msgr_ci->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_msgr_ci->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_msgr_ci->pfnUserCallback = debug_callback;
}

Nob_Log_Level translate_msg_severity(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity)
{
    switch (msg_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return NOB_INFO;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return NOB_INFO;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return NOB_WARNING;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return NOB_ERROR;
    default: assert(0 && "this message severity is not handled");
    }
}

bool setup_debug_msgr()
{
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
    populated_debug_msgr_ci(&debug_msgr_ci);
    load_pfn(vkCreateDebugUtilsMessengerEXT);
    if (vkCreateDebugUtilsMessengerEXT) {
        return VK_SUCCEEDED(vkCreateDebugUtilsMessengerEXT(ctx.instance, &debug_msgr_ci, NULL, &ctx.debug_msgr));
    } else {
        nob_log(NOB_ERROR, "failed to load function pointer for vkCreateDebugUtilesMessenger");
        return false;
    }
}

Queue_Fams find_queue_fams(VkPhysicalDevice phys_device)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fam_props[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, queue_fam_props);
    Queue_Fams queue_fams = {0};
    for (size_t i = 0; i < queue_fam_count; i++) {
        if (queue_fam_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_fams.gfx_idx = i;
            queue_fams.has_gfx = true;
        }

        if (queue_fam_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queue_fams.compute_idx = i;
            queue_fams.has_compute = true;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, ctx.surface, &present_support);
        if (present_support) {
            queue_fams.present_idx = i;
            queue_fams.has_present = true;
        }

        if (queue_fams.has_gfx && queue_fams.has_present && queue_fams.has_compute)
            return queue_fams;
    }
    return queue_fams;
}

bool pick_phys_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, NULL);
    VkPhysicalDevice phys_devices[device_count];
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, phys_devices);
    for (size_t i = 0; i < device_count; i++) {
        if (is_device_suitable(phys_devices[i])) {
            ctx.phys_device = phys_devices[i];
            return true;
        }
    }

    return false;
}

void populate_set(int arr[], size_t arr_size, U32_Set *set)
{
    for (size_t i = 0; i < arr_size; i++) {
        if (arr[i] == -1)
            continue;

        nob_da_append(set, arr[i]);

        for (size_t j = i + 1; j < arr_size; j++)
            if (arr[i] == arr[j])
                arr[j] = -1;
    }
}

bool swapchain_adequate(VkPhysicalDevice phys_device)
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, ctx.surface, &surface_fmt_count, NULL);
    if (!surface_fmt_count) return false;
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, ctx.surface, &present_mode_count, NULL);
    if (!present_mode_count) return false;

    return true;
}

VkSurfaceFormatKHR choose_swapchain_fmt()
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.phys_device, ctx.surface, &surface_fmt_count, NULL);
    VkSurfaceFormatKHR fmts[surface_fmt_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.phys_device, ctx.surface, &surface_fmt_count, fmts);
    for (size_t i = 0; i < surface_fmt_count; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmts[i];
    }

    assert(surface_fmt_count && "surface format count was zero");
    return fmts[0];
}

VkPresentModeKHR choose_present_mode()
{
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.phys_device, ctx.surface, &present_mode_count, NULL);
    VkPresentModeKHR present_modes[present_mode_count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.phys_device, ctx.surface, &present_mode_count, present_modes);
    for (size_t i = 0; i < present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_modes[i];
    }

    assert(present_mode_count && "present mode count was zero");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swp_extent()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.phys_device, ctx.surface, &capabilities);
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(ctx.window, &width, &height);

        VkExtent2D extent = {
            .width = width,
            .height = height
        };

        extent.width = clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

void cleanup_swapchain()
{
    vkDestroyImageView(ctx.device, ctx.depth_img_view, NULL);
    vkDestroyImage(ctx.device, ctx.depth_img.handle, NULL);
    vkFreeMemory(ctx.device, ctx.depth_img.mem, NULL);

    for (size_t i = 0; i < ctx.swapchain.buffs.count; i++)
        vkDestroyFramebuffer(ctx.device, ctx.swapchain.buffs.items[i], NULL);
    for (size_t i = 0; i < ctx.swapchain.img_views.count; i++)
        vkDestroyImageView(ctx.device, ctx.swapchain.img_views.items[i], NULL);
    vkDestroySwapchainKHR(ctx.device, ctx.swapchain.handle, NULL);

    nob_da_reset(ctx.swapchain.buffs);
    nob_da_reset(ctx.swapchain.img_views);
    nob_da_reset(ctx.swapchain.imgs);
}

bool find_mem_type_idx(uint32_t type, VkMemoryPropertyFlags properties, uint32_t *idx)
{
    VkPhysicalDeviceMemoryProperties mem_properites = {0};
    vkGetPhysicalDeviceMemoryProperties(ctx.phys_device, &mem_properites);
    for (uint32_t i = 0; i < mem_properites.memoryTypeCount; i++) {
        if (type & (1 << i) && (mem_properites.memoryTypes[i].propertyFlags & properties) == properties) {
            *idx = i;
            return true;
        }
    }

    return false;
}

void init_ext_managner()
{
    if (ext_manager.inited) {
        nob_log(NOB_WARNING, "extension manager already initialized");
        return;
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(validation_layers); i++)
        nob_da_append(&ext_manager.validation_layers, validation_layers[i]);

    for (size_t i = 0; i < NOB_ARRAY_LEN(device_exts); i++)
        nob_da_append(&ext_manager.device_exts, device_exts[i]);

    ext_manager.inited = true;
}

void destroy_ext_manager()
{
    if (!ext_manager.inited) {
        nob_log(NOB_WARNING, "extension manager was not initialized");
        return;
    }

    nob_da_reset(ext_manager.validation_layers);
    nob_da_reset(ext_manager.device_exts);
    nob_da_reset(ext_manager.inst_exts);
}

bool inst_exts_satisfied()
{
    uint32_t avail_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, avail_exts);
    size_t unsatisfied_exts = ext_manager.inst_exts.count;
    for (size_t i = 0; i < ext_manager.inst_exts.count; i++) {
        bool found = false;
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(ext_manager.inst_exts.items[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
                found = true;
                break;
            }
        }
        if (!found)
            nob_log(NOB_ERROR, "Instance extension `%s` not available", ext_manager.inst_exts.items[i]);
    }

    return false;
}

bool chk_validation_support()
{
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties avail_layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, avail_layers);
    size_t unsatisfied_layers = ext_manager.validation_layers.count;
    for (size_t i = 0; i < ext_manager.validation_layers.count; i++) {
        bool found = false;
        for (size_t j = 0; j < layer_count; j++) {
            if (strcmp(ext_manager.validation_layers.items[i], avail_layers[j].layerName) == 0) {
                if (--unsatisfied_layers == 0)
                    return true;
                found = true;
                break;
            }
        }
        if (!found)
            nob_log(NOB_ERROR, "Validation layer `%s` not available", ext_manager.validation_layers.items[i]);
    }

    return true;
}

bool device_exts_supported(VkPhysicalDevice phys_device)
{
    uint32_t avail_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &avail_ext_count, avail_exts);
    uint32_t unsatisfied_exts = ext_manager.device_exts.count; 
    for (size_t i = 0; i < ext_manager.device_exts.count; i++) {
        bool found = false;
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(ext_manager.device_exts.items[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
                found = true;
                break;
            }
        }
        if (!found)
            nob_log(NOB_ERROR, "Device extension `%s` not available", ext_manager.device_exts.items[i]);
    }

    return false;
}

VkVertexInputBindingDescription get_binding_desc(Pipeline_Type pipeline_type)
{
    VkVertexInputBindingDescription bindingDescription = {0};
    if (pipeline_type == PIPELINE_POINT_CLOUD || pipeline_type == PIPELINE_POINT_CLOUD_ADV) {
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Small_Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    } else if (pipeline_type == PIPELINE_COMPUTE) {
        bindingDescription.binding = 0;
        bindingDescription.stride = 32; // TODO: this is hardcoded
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    } else {
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    return bindingDescription;
}

void get_attr_descs(VtxAttrDescs *attr_descs, Pipeline_Type pipeline_type)
{
    VkVertexInputAttributeDescription desc = {0};

    /* for point clouds use small vertex */
    if (pipeline_type == PIPELINE_POINT_CLOUD ||
        pipeline_type == PIPELINE_POINT_CLOUD_ADV) {
        desc.binding = 0;
        desc.location = 0;
        desc.format = VK_FORMAT_R32G32B32_SFLOAT;
        desc.offset = 0;
        nob_da_append(attr_descs, desc);

        desc.binding = 0;
        desc.location = 1;
        desc.format = VK_FORMAT_R8G8B8A8_UINT;
        desc.offset = 12;
        nob_da_append(attr_descs, desc);
    } else if (pipeline_type == PIPELINE_COMPUTE) {
        desc.binding = 0;
        desc.location = 0;
        desc.format = VK_FORMAT_R32G32_SFLOAT;
        desc.offset = 0;
        nob_da_append(attr_descs, desc);

        desc.binding = 0;
        desc.location = 1;
        desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        desc.offset = 16;
        nob_da_append(attr_descs, desc);
    } else {
        desc.binding = 0;
        desc.location = 0;
        desc.format = VK_FORMAT_R32G32B32_SFLOAT;
        desc.offset = offsetof(Vertex, pos);
        nob_da_append(attr_descs, desc);

        desc.binding = 0;
        desc.location = 1;
        desc.format = VK_FORMAT_R32G32B32_SFLOAT;
        desc.offset = offsetof(Vertex, color);
        nob_da_append(attr_descs, desc);

        desc.binding = 0;
        desc.location = 2;
        desc.format = VK_FORMAT_R32G32_SFLOAT;
        desc.offset = offsetof(Vertex, tex_coord);
        nob_da_append(attr_descs, desc);
    }
}

bool vk_buff_init(Vk_Buffer *buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    bool result = true;

    VkBufferCreateInfo buffer_ci = {0};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cvr_chk(buffer->size, "Vk_Buffer must be set with size before calling vk_buff_init");
    buffer_ci.size = (VkDeviceSize) buffer->size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_chk(vkCreateBuffer(ctx.device, &buffer_ci, NULL, &buffer->handle), "failed to create buffer");

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(ctx.device, buffer->handle, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(ctx.device, &alloc_ci, NULL, &buffer->mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");
    vk_chk(vkBindBufferMemory(ctx.device, buffer->handle, buffer->mem, 0), "failed to bind buffer memory");

defer:
    return result;
}

void vk_buff_destroy(Vk_Buffer buffer)
{
    vkDestroyBuffer(ctx.device, buffer.handle, NULL);
    vkFreeMemory(ctx.device, buffer.mem, NULL);
    buffer.handle = NULL;
}

bool vk_vtx_buff_staged_upload(Vk_Buffer *vtx_buff, const void *data)
{
    bool result = true;

    /* create two buffers, a so-called "staging" buffer, and one for our actual vertex buffer */
    Vk_Buffer stg_buff = {
        .size   = vtx_buff->size,
        .count  = vtx_buff->count,
    };
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    vk_chk(vkMapMemory(ctx.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped), "failed to map memory");
    memcpy(stg_buff.mapped, data, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.mem);

    result = vk_buff_init(
        vtx_buff,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create vertex buffer");

    /* transfer data from staging buffer to vertex buffer */
    vk_buff_copy(*vtx_buff, stg_buff, 0);

defer:
    vk_buff_destroy(stg_buff);
    return result;
}

bool vk_vtx_buff_upload(Vk_Buffer *vtx_buff, const void *data)
{
    bool result = vk_buff_init(
        vtx_buff,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (!result) return false;

    VkResult res = vkMapMemory(ctx.device, vtx_buff->mem, 0, vtx_buff->size, 0, &(vtx_buff->mapped));
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to map memory");
        return false;
    }

    memcpy(vtx_buff->mapped, data, vtx_buff->size);
    vkUnmapMemory(ctx.device, vtx_buff->mem);

    return true;
}

bool vk_comp_buff_staged_upload(Vk_Buffer *vtx_buff, const void *data)
{
    bool result = true;

    /* create two buffers, a so-called "staging" buffer, and one for our actual vertex compute buffer */
    Vk_Buffer stg_buff = {
        .size   = vtx_buff->size,
        .count  = vtx_buff->count,
    };
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging compute buffer");

    vk_chk(vkMapMemory(ctx.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped), "failed to map memory");
    memcpy(stg_buff.mapped, data, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.mem);

    result = vk_buff_init(
        vtx_buff,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT  |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create compute buffer");

    /* transfer data from staging buffer to vertex buffer */
    vk_buff_copy(*vtx_buff, stg_buff, 0);

defer:
    vk_buff_destroy(stg_buff);
    return result;
}

bool vk_idx_buff_staged_upload(Vk_Buffer *idx_buff, const void *data)
{
    bool result = true;

    /* create two buffers, a so-called "staging" buffer, and one for our actual index buffer */
    Vk_Buffer stg_buff = {
        .size   = idx_buff->size,
        .count  = idx_buff->count,
    };
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    vk_chk(vkMapMemory(ctx.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped), "failed to map memory");
    memcpy(stg_buff.mapped, data, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.mem);

    result = vk_buff_init(
        idx_buff,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create index buffer");

    /* transfer data from staging buffer to index buffer */
    vk_buff_copy(*idx_buff, stg_buff, 0);

defer:
    vk_buff_destroy(stg_buff);
    return result;
}

bool vk_idx_buff_upload(Vk_Buffer *idx_buff, const void *data)
{
    bool result = vk_buff_init(
        idx_buff,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (!result) return false;

    VkResult res = vkMapMemory(ctx.device, idx_buff->mem, 0, idx_buff->size, 0, &(idx_buff->mapped));
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to map memory in idx buff upload");
        return false;
    }
    memcpy(idx_buff->mapped, data, idx_buff->size);
    vkUnmapMemory(ctx.device, idx_buff->mem);

    return true;
}


bool vk_buff_copy(Vk_Buffer dst_buff, Vk_Buffer src_buff, VkDeviceSize size)
{
    VkCommandBuffer tmp_cmd_buff;
    if (!cmd_quick_begin(&tmp_cmd_buff)) return false;

    VkBufferCopy copy_region = {0};
    if (size) {
        copy_region.size = size;
        if (size > dst_buff.size) {
            nob_log(NOB_ERROR, "Cannot copy buffer, size > dst buffer (won't fit)");
            return false;
        }
        if (size > src_buff.size) {
            nob_log(NOB_ERROR, "Cannot copy buffer, size > src buffer (cannot copy more than what's available)");
            return false;
        }
    } else {
        if (dst_buff.size < src_buff.size) {
            nob_log(NOB_ERROR, "Cannot copy buffer, dst buffer < src buffer (won't fit)");
            return false;
        }
        copy_region.size = src_buff.size;
    }

    vkCmdCopyBuffer(tmp_cmd_buff, src_buff.handle, dst_buff.handle, 1, &copy_region);
    if (!cmd_quick_end(&tmp_cmd_buff)) return false;

    return true;
}

bool vk_img_init(Vk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    bool result = true;

    if (img->extent.width == 0 && img->extent.height == 0) {
        nob_log(NOB_ERROR, "Vk_Image must be set with width/height before calling vk_img_init");
        nob_return_defer(false);
    }

    VkImageCreateInfo img_ci = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = img->format,
        .extent      = {img->extent.width, img->extent.height, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    vk_chk(vkCreateImage(ctx.device, &img_ci, NULL, &img->handle), "failed to create image");

    VkMemoryRequirements mem_reqs = {0};
    vkGetImageMemoryRequirements(ctx.device, img->handle, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
    };
    if (!find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex)) {
        nob_log(NOB_ERROR, "Memory not suitable based on memory requirements");
        nob_return_defer(false);
    }
    VkResult res = vkAllocateMemory(ctx.device, &alloc_ci, NULL, &img->mem);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to allocate buffer memory!");
        nob_return_defer(false);
    }

    res = vkBindImageMemory(ctx.device, img->handle, img->mem, 0);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to bind image buffer memory");
        nob_return_defer(false);
    }

defer:
    return result;
}

bool cmd_man_init()
{
    if (!cmd_pool_create())  return false;
    if (!cmd_buff_create())  return false;
    if (!cmd_syncs_create()) return false;
    return true;
}

bool cmd_pool_create()
{
    Queue_Fams queue_fams = find_queue_fams(ctx.phys_device);
    if (!queue_fams.has_gfx) {
        nob_log(NOB_ERROR, "failed to create command pool, no graphics queue");
        return false;
    }

    VkCommandPoolCreateInfo cmd_pool_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_fams.gfx_idx,
    };
    VkResult res = vkCreateCommandPool(ctx.device, &cmd_pool_ci, NULL, &cmd_man.pool);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create command pool");
        return false;
    }

    return true;
}

bool cmd_buff_create()
{
    VkCommandBufferAllocateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_man.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkResult res = vkAllocateCommandBuffers(ctx.device, &ci, &cmd_man.gfx_buff);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create graphics command buffer");
        return false;
    }

    res = vkAllocateCommandBuffers(ctx.device, &ci, &cmd_man.compute_buff);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create compute command buffer");
        return false;
    }

    return true;
}

void cmd_man_destroy()
{
    vkDestroySemaphore(ctx.device, cmd_man.img_avail_sem, NULL);
    vkDestroySemaphore(ctx.device, cmd_man.render_fin_sem, NULL);
    vkDestroySemaphore(ctx.device, cmd_man.compute_fin_sem, NULL);
    vkDestroyFence(ctx.device, cmd_man.fence, NULL);
    vkDestroyFence(ctx.device, cmd_man.compute_fence, NULL);
    vkDestroyCommandPool(ctx.device, cmd_man.pool, NULL);
}

bool cmd_syncs_create()
{
    VkSemaphoreCreateInfo sem_ci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkResult res = VK_SUCCESS;
    res |= vkCreateSemaphore(ctx.device, &sem_ci, NULL, &cmd_man.img_avail_sem);
    res |= vkCreateSemaphore(ctx.device, &sem_ci, NULL, &cmd_man.render_fin_sem);
    res |= vkCreateSemaphore(ctx.device, &sem_ci, NULL, &cmd_man.compute_fin_sem);
    res |= vkCreateFence(ctx.device, &fence_ci, NULL, &cmd_man.fence);
    res |= vkCreateFence(ctx.device, &fence_ci, NULL, &cmd_man.compute_fence);

    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create sync objects");
        return false;
    }

    return true;
}

bool cmd_quick_begin(VkCommandBuffer *tmp_cmd_buff)
{
    VkCommandBufferAllocateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = cmd_man.pool,
        .commandBufferCount = 1,
    };
    VkResult res = vkAllocateCommandBuffers(ctx.device, &ci, tmp_cmd_buff);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create quick cmd");
        return false;
    }
    VkCommandBufferBeginInfo cmd_begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    res = vkBeginCommandBuffer(*tmp_cmd_buff, &cmd_begin);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to begin quick cmd");
        return false;
    }

    return true;
}

bool cmd_quick_end(VkCommandBuffer *tmp_cmd_buff)
{
    bool result = true;

    VkResult res = vkEndCommandBuffer(*tmp_cmd_buff);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to end cmd buffer");
        nob_return_defer(false);
    }

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = tmp_cmd_buff,
    };

    res = vkQueueSubmit(ctx.gfx_queue, 1, &submit, VK_NULL_HANDLE);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to submit quick cmd");
        nob_return_defer(false);
    }

    /* TODO: perhaps I should use a fence instead of a queue wait idle */

    res = vkQueueWaitIdle(ctx.gfx_queue);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to wait idle in quick cmd");
        nob_return_defer(false);
    }

defer:
    vkFreeCommandBuffers(ctx.device, cmd_man.pool, 1, tmp_cmd_buff);
    return result;
}

bool transition_img_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    bool result = true;

    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(&tmp_cmd_buff), "failed quick cmd begin");

    VkPipelineStageFlags src_stg_mask;
    VkPipelineStageFlags dst_stg_mask;
    VkAccessFlags src_access_mask;
    VkAccessFlags dst_access_mask;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        src_access_mask = 0;
        dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stg_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stg_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_access_mask = VK_ACCESS_SHADER_READ_BIT;
        src_stg_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stg_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        src_access_mask = 0;
        dst_access_mask = 0;
        src_stg_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dst_stg_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    } else {
        nob_log(NOB_ERROR, "old_layout %d with new_layout %d not allowed yet", old_layout, new_layout);
        nob_return_defer(false);
    }

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
        tmp_cmd_buff, src_stg_mask, dst_stg_mask,
        0, 0, NULL, 0, NULL, 1,
        &barrier
    );

    cvr_chk(cmd_quick_end(&tmp_cmd_buff), "failed quick cmd end");

defer:
    return result;
}

bool vk_img_copy(VkImage dst_img, VkBuffer src_buff, VkExtent2D extent)
{
    bool result = true;
    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(&tmp_cmd_buff), "failed quick cmd begin");

    VkBufferImageCopy region = {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {extent.width, extent.height, 1},
    };

    vkCmdCopyBufferToImage(
        tmp_cmd_buff,
        src_buff,
        dst_img,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    cvr_chk(cmd_quick_end(&tmp_cmd_buff), "failed quick cmd end");

defer:
    return result;
}

static inline int format_to_size(VkFormat fmt)
{
    if (fmt == VK_FORMAT_R8G8B8A8_SRGB) {
        return 4;
    } else {
        nob_log(NOB_WARNING, "unrecognized format %d, returning 4 instead", fmt);
        return 4;
    }
}

bool vk_load_texture(void *data, size_t width, size_t height, VkFormat fmt, size_t *id, Descriptor_Type ds_type)
{
    bool result = true;

    /* create staging buffer for image */
    Vk_Buffer stg_buff;
    stg_buff.size  = width * height * format_to_size(fmt);
    stg_buff.count = width * height;
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if(!result) {
        nob_log(NOB_ERROR, "failed to create staging buffer");
        nob_return_defer(false);
    }
    VkResult res = vkMapMemory(ctx.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped);
    vk_chk(res, "unable to map memory");
    memcpy(stg_buff.mapped, data, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.mem);

    /* create the image */
    Vk_Image vk_img = {
        .extent  = {width, height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = fmt,
    };
    result = vk_img_init(
        &vk_img,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create image");

    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    vk_img_copy(vk_img.handle, stg_buff.handle, vk_img.extent);
    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    /* create image view */
    VkImageView img_view;
    result = vk_img_view_init(vk_img, &img_view);
    cvr_chk(result, "failed to create image view");

    /* create sampler */
    VkSamplerCreateInfo sampler_ci = {0};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties props = {0};
    vkGetPhysicalDeviceProperties(ctx.phys_device, &props);
    sampler_ci.maxAnisotropy = props.limits.maxSamplerAnisotropy;
    sampler_ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_ci.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSampler sampler;
    vk_chk(vkCreateSampler(ctx.device, &sampler_ci, NULL, &sampler), "failed to create sampler");

    Vk_Texture_Set *texture_set = &ctx.texture_sets[ds_type];
    Vk_Texture texture = {
        .view = img_view,
        .sampler = sampler,
        .img = vk_img,
        .id = texture_set->count,
        .active = true,
    };

    *id = texture.id;
    nob_da_append(texture_set, texture);

defer:
    vk_buff_destroy(stg_buff);
    return result;
}

bool vk_unload_texture(size_t id, Descriptor_Type ds_type)
{
    vkDeviceWaitIdle(ctx.device);

    Vk_Texture_Set texture_set = ctx.texture_sets[ds_type];
    for (size_t i = 0; i < texture_set.count; i++) {
        Vk_Texture *texture = &texture_set.items[i];
        if (texture->id == id) {
            if (!texture->active) {
                nob_log(NOB_WARNING, "cannot unload texture %d, already inactive", texture->id);
                return false;
            } else {
                vkDestroySampler(ctx.device, texture->sampler, NULL);
                vkDestroyImageView(ctx.device, texture->view, NULL);
                vkDestroyImage(ctx.device, texture->img.handle, NULL);
                vkFreeMemory(ctx.device, texture->img.mem, NULL);
                texture->active = false;
            }
        }
    }

    return true;
}

void vk_unload_texture2(Vk_Texture texture)
{
    vkDestroySampler(ctx.device, texture.sampler, NULL);
    vkDestroyImageView(ctx.device, texture.view, NULL);
    vkDestroyImage(ctx.device, texture.img.handle, NULL);
    vkFreeMemory(ctx.device, texture.img.mem, NULL);
}

bool vk_create_storage_img(Vk_Texture *texture)
{
    if (texture->img.extent.width == 0 && texture->img.extent.height == 0) {
        nob_log(NOB_ERROR, "storage image does not have width or height");
        return false;
    }

    /* setup storage image */
    Vk_Image vk_img = {
        .extent  = texture->img.extent,
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    };

    bool result = vk_img_init(
        &vk_img,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (!result) return false;

    transition_img_layout(
        vk_img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL
    );

    /* here we could also check if the graphics and compute queues are the same
     * if they are not the same then we can make an image memory barrier
     * for now I will skip this */

    /* create image view */
    VkImageView img_view;
    if (!vk_img_view_init(vk_img, &img_view)) return false;

    /* create sampler */
    VkPhysicalDeviceProperties props = {0};
    vkGetPhysicalDeviceProperties(ctx.phys_device, &props);
    VkSamplerCreateInfo sampler_ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = props.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };
    VkSampler sampler;
    VkResult res = vkCreateSampler(ctx.device, &sampler_ci, NULL, &sampler);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create sampler");
        return false;
    }

    texture->view = img_view;
    texture->sampler = sampler;
    texture->img = vk_img;

    return true;
}

bool vk_create_ds_layout(VkDescriptorSetLayoutCreateInfo layout_ci, VkDescriptorSetLayout *layout)
{
    VkResult res = vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, layout);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor set layout");
        return false;
    }

    return true;
}

bool vk_create_ds_pool(VkDescriptorPoolCreateInfo pool_ci, VkDescriptorPool *pool)
{
    VkResult res = vkCreateDescriptorPool(ctx.device, &pool_ci, NULL, pool);
    if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor pool");
        return false;
    }

    return true;
}

void vk_destroy_ds_pool(VkDescriptorPool pool)
{
    vkDestroyDescriptorPool(ctx.device, pool, NULL);
}

void vk_destroy_ds_layout(VkDescriptorSetLayout layout)
{
    vkDestroyDescriptorSetLayout(ctx.device, layout, NULL);
}

bool vk_alloc_ds(VkDescriptorSetAllocateInfo alloc, VkDescriptorSet *sets)
{
    VkResult res = vkAllocateDescriptorSets(ctx.device, &alloc, sets);
    if (res == VK_ERROR_OUT_OF_POOL_MEMORY) {
        nob_log(NOB_ERROR, "out of pool memory");
        return false;
    } else if (!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to allocate descriptor sets");
        return false;
    }

    return true;
}

void vk_update_ds(size_t count, VkWriteDescriptorSet *writes)
{
    vkUpdateDescriptorSets(ctx.device, (uint32_t)count, writes, 0, NULL);
}

#endif // VK_CTX_IMPLEMENTATION
