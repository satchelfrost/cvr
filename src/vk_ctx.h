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
#define vk_ok(x) ((x) == VK_SUCCESS)
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
#define vk_chk(vk_result, msg) cvr_chk(vk_ok(vk_result), msg)
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
    bool active;
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

typedef enum {
    PIPELINE_DEFAULT = 0,
    PIPELINE_WIREFRAME,
    PIPELINE_POINT_CLOUD,
    PIPELINE_POINT_CLOUD_ADV,
    PIPELINE_TEXTURE,
    PIPELINE_COMPUTE,
    PIPELINE_COUNT,
} Pipeline_Type;

/* TODO: need to consolodate these different types */
/* I'm thinking we can get more general with the names e.g. SET_LAYOUT_FOUR_TEXTURES, SET_LAYOUT_SINGLE_TEXTURE */
typedef enum {
    SET_LAYOUT_TEX_UBO = 0,
    SET_LAYOUT_TEX_SAMPLER,
    SET_LAYOUT_ADV_POINT_CLOUD_UBO,
    SET_LAYOUT_ADV_POINT_CLOUD_SAMPLER,
    SET_LAYOUT_COUNT,
} Set_Layout_Type;

typedef enum {
    POOL_TEX_UBO = 0,
    POOL_TEX_SAMPLER,
    POOL_ADV_POINT_CLOUD_UBO,
    POOL_ADV_POINT_CLOUD_SAMPLER,
    POOL_COUNT,
} Descriptor_Pool_Type;

typedef enum {
    UBO_TYPE_TEX,
    UBO_TYPE_ADV_POINT_CLOUD,
    UBO_TYPE_COMPUTE,
    UBO_TYPE_COUNT,
} UBO_Type;

typedef struct {
    VkDescriptorSetLayout *items;
    size_t count;
    size_t capacity;
} Descriptor_Set_Layouts;

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
    Vk_Texture_Set texture_sets[SET_LAYOUT_COUNT];
    UBO ubos[UBO_TYPE_COUNT];
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
bool vk_compute_pl_init();
bool vk_shader_mod_init(const char *file_name, VkShaderModule *module);
bool vk_render_pass_init();
bool vk_frame_buffs_init();
bool vk_recreate_swapchain();
bool vk_depth_init();

/* general ubo initializer */
bool vk_ubo_init(Vk_Buffer *buff);
bool vk_ubo_descriptor_set_layout_init(VkShaderStageFlags flags, uint32_t binding, VkDescriptorSetLayout *layout);
bool vk_ubo_descriptor_set_init(UBO *ubo);
bool vk_ubo_descriptor_pool(VkDescriptorPool *pool);
bool vk_sampler_descriptor_pool_init(Vk_Texture_Set *texture_set);
bool vk_sampler_descriptor_set_layout_init(Set_Layout_Type layout_type);
bool vk_sampler_descriptor_set_init(Vk_Texture_Set *texture_set, bool group_textures);

/* Manages synchronization info and gets ready for vulkan commands. */
bool vk_begin_drawing();

/* Submits vulkan commands. */
bool vk_end_drawing();

bool vk_draw(Pipeline_Type pipeline_type, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix mvp);
bool vk_draw_points(Vk_Buffer vtx_buff, Matrix mvp, Example example);
bool vk_draw_texture(size_t id, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix mvp);

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
    VkCommandBuffer buff;
    VkSemaphore img_avail_sem;
    VkSemaphore render_fin_sem;
    VkFence fence;
} Vk_Cmd_Man;
static Vk_Cmd_Man cmd_man = {0};

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

bool vk_img_init(Vk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
bool vk_img_copy(VkImage dst_img, VkBuffer src_buff, VkExtent2D extent);
bool vk_load_texture(void *data, size_t width, size_t height, VkFormat fmt, size_t *id, bool pc_texture);
bool vk_unload_texture(size_t id);
bool vk_unload_pc_texture(size_t id);

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

/* current image index (only zero if there's only one image) */
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
    for (size_t i = 0; i < SET_LAYOUT_COUNT; i++) {
        Vk_Texture_Set texture_set = ctx.texture_sets[i];
        vkDestroyDescriptorPool(ctx.device, texture_set.descriptor_pool, NULL);
    }
    for (size_t i = 0; i < SET_LAYOUT_COUNT; i++) {
        Vk_Texture_Set texture_set = ctx.texture_sets[i];
        vkDestroyDescriptorSetLayout(ctx.device, texture_set.set_layout, NULL);
    }

    for (size_t i = 0; i < PIPELINE_COUNT; i++) {
        vkDestroyPipeline(ctx.device, ctx.pipelines[i], NULL);
        vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layouts[i], NULL);
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
    app_info.apiVersion = VK_API_VERSION_1_0;

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

    VkDeviceCreateInfo device_ci = {0};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    VkPhysicalDeviceFeatures features = {0};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    device_ci.pEnabledFeatures = &features;
    device_ci.pQueueCreateInfos = queue_cis.items;
    device_ci.queueCreateInfoCount = queue_cis.count;
    device_ci.enabledExtensionCount = ext_manager.device_exts.count;
    device_ci.ppEnabledExtensionNames = ext_manager.device_exts.items;
#ifdef ENABLE_VALIDATION
    device_ci.enabledLayerCount = ext_manager.validation_layers.count;
    device_ci.ppEnabledLayerNames = ext_manager.validation_layers.items;
#endif

    bool result = true;
    if (vk_ok(vkCreateDevice(ctx.phys_device, &device_ci, NULL, &ctx.device))) {
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
    return vk_ok(glfwCreateWindowSurface(ctx.instance, ctx.window, NULL, &ctx.surface));
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

    if (vk_ok(vkCreateSwapchainKHR(ctx.device, &swapchain_ci, NULL, &ctx.swapchain.handle))) {
        uint32_t img_count = 0;
        vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain.handle, &img_count, NULL);
        nob_da_resize(&ctx.swapchain.imgs, img_count);
        vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain.handle, &img_count, ctx.swapchain.imgs.items);
        return true;
    } else {
        return false;
    }
}

bool vk_img_view_init(Vk_Image img, VkImageView *img_view)
{
    VkImageViewCreateInfo img_view_ci = {0};
    img_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    img_view_ci.image = img.handle;
    img_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    img_view_ci.format = img.format;
    img_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    img_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    img_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    img_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    img_view_ci.subresourceRange.aspectMask = img.aspect_mask;
    img_view_ci.subresourceRange.baseMipLevel = 0;
    img_view_ci.subresourceRange.levelCount = 1;
    img_view_ci.subresourceRange.baseArrayLayer = 0;
    img_view_ci.subresourceRange.layerCount = 1;
    return vk_ok(vkCreateImageView(ctx.device, &img_view_ci, NULL, img_view));
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

bool vk_basic_pl_init(Pipeline_Type pipeline_type)
{
    bool result = true;

    char *vert_shader_name;
    char *frag_shader_name;
    switch (pipeline_type) {
    case PIPELINE_TEXTURE:
        vert_shader_name = "./res/texture.vert.spv";
        frag_shader_name = "./res/texture.frag.spv";
        break;
    case PIPELINE_POINT_CLOUD_ADV:
    case PIPELINE_POINT_CLOUD:
        vert_shader_name = "./res/point-cloud.vert.spv";
        frag_shader_name = "./res/point-cloud.frag.spv";
        break;
    case PIPELINE_DEFAULT:
    case PIPELINE_WIREFRAME:
    default:
        vert_shader_name = "./res/default.vert.spv";
        frag_shader_name = "./res/default.frag.spv";
        break;
    }

    VkPipelineShaderStageCreateInfo vert_ci = {0};
    vert_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_ci.pName = "main";
    if (!vk_shader_mod_init(vert_shader_name, &vert_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo frag_ci = {0};
    frag_ci .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_ci .stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_ci.pName = "main";
    if (!vk_shader_mod_init(frag_shader_name, &frag_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo stages[] = {vert_ci, frag_ci};

    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {0};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    dynamic_state_ci.dynamicStateCount = NOB_ARRAY_LEN(dynamic_states);
    dynamic_state_ci.pDynamicStates = dynamic_states;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {0};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VtxAttrDescs vert_attrs = {0};
    get_attr_descs(&vert_attrs, pipeline_type);
    VkVertexInputBindingDescription binding_desc = get_binding_desc(pipeline_type);
    vertex_input_ci.vertexBindingDescriptionCount = 1;
    vertex_input_ci.pVertexBindingDescriptions = &binding_desc;
    vertex_input_ci.vertexAttributeDescriptionCount = vert_attrs.count;
    vertex_input_ci.pVertexAttributeDescriptions = vert_attrs.items;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {0};
    input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    if (pipeline_type == PIPELINE_POINT_CLOUD || pipeline_type == PIPELINE_POINT_CLOUD_ADV)
        input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    else
        input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0};
    viewport.width = (float) ctx.extent.width;
    viewport.height = (float) ctx.extent.height;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {0};
    scissor.extent = ctx.extent;
    VkPipelineViewportStateCreateInfo viewport_state_ci = {0};
    viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.viewportCount = 1;
    viewport_state_ci.pViewports = &viewport;
    viewport_state_ci.scissorCount = 1;
    viewport_state_ci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {0};
    rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_ci.polygonMode = (pipeline_type == PIPELINE_WIREFRAME) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer_ci.lineWidth = 1.0f;
    rasterizer_ci.cullMode = VK_CULL_MODE_NONE;
    rasterizer_ci.lineWidth = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling_ci = {0};
    multisampling_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend = {0};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                 VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT |
                                 VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo color_blend_ci = {0};
    color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_ci.attachmentCount = 1;
    color_blend_ci.pAttachments = &color_blend;

    VkPipelineLayoutCreateInfo pipeline_layout_ci = {0};
    pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    Descriptor_Set_Layouts set_layouts = {0};
    if (pipeline_type == PIPELINE_TEXTURE) {
        nob_da_append(&set_layouts, ctx.ubos[UBO_TYPE_TEX].set_layout);
        nob_da_append(&set_layouts, ctx.texture_sets[SET_LAYOUT_TEX_SAMPLER].set_layout);
    } else if (pipeline_type == PIPELINE_POINT_CLOUD_ADV) {
        nob_da_append(&set_layouts, ctx.ubos[UBO_TYPE_ADV_POINT_CLOUD].set_layout);
        nob_da_append(&set_layouts, ctx.texture_sets[SET_LAYOUT_ADV_POINT_CLOUD_SAMPLER].set_layout);
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
    VkResult vk_result = vkCreatePipelineLayout(
        ctx.device,
        &pipeline_layout_ci,
        NULL,
        &ctx.pipeline_layouts[pipeline_type]
    );
    vk_chk(vk_result, "failed to create pipeline layout");
    VkPipelineDepthStencilStateCreateInfo depth_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .maxDepthBounds = 1.0f,
    };

    VkGraphicsPipelineCreateInfo pipeline_ci = {0};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = NOB_ARRAY_LEN(stages);
    pipeline_ci.pStages = stages;
    pipeline_ci.pVertexInputState = &vertex_input_ci;
    pipeline_ci.pInputAssemblyState = &input_assembly_ci;
    pipeline_ci.pViewportState = &viewport_state_ci;
    pipeline_ci.pRasterizationState = &rasterizer_ci;
    pipeline_ci.pMultisampleState = &multisampling_ci;
    pipeline_ci.pColorBlendState = &color_blend_ci;
    pipeline_ci.pDynamicState = &dynamic_state_ci;
    pipeline_ci.pDepthStencilState = &depth_ci;
    pipeline_ci.layout = ctx.pipeline_layouts[pipeline_type];
    pipeline_ci.renderPass = ctx.render_pass;
    pipeline_ci.subpass = 0;

    vk_result = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &ctx.pipelines[pipeline_type]);
    vk_chk(vk_result, "failed to create pipeline");

defer:
    vkDestroyShaderModule(ctx.device, frag_ci.module, NULL);
    vkDestroyShaderModule(ctx.device, vert_ci.module, NULL);
    nob_da_free(vert_attrs);
    nob_da_free(set_layouts);
    return result;
}

bool vk_compute_pl_init()
{
    bool result = true;

    VkPipelineShaderStageCreateInfo shader_ci = {0};
    shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_ci.pName = "main";
    if (!vk_shader_mod_init("./res/default.comp.spv", &shader_ci.module))
        nob_return_defer(false);

    VkPipelineLayoutCreateInfo pipeline_layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx.ubos[UBO_TYPE_COMPUTE].set_layout,
    };
    VkResult res = vkCreatePipelineLayout(
        ctx.device,
        &pipeline_layout_ci,
        NULL,
        &ctx.pipeline_layouts[PIPELINE_COMPUTE]
    );
    if (!vk_ok(res)) {
        nob_log(NOB_ERROR, "failed to create layout for compute pipeline");
        nob_return_defer(false);
    }

    VkComputePipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = ctx.pipeline_layouts[PIPELINE_COMPUTE],
        .stage = shader_ci,
    };
    res = vkCreateComputePipelines(
        ctx.device,
        VK_NULL_HANDLE,
        1,
        &pipeline_ci,
        NULL,
        &ctx.pipelines[PIPELINE_COMPUTE]
    );
    if (!vk_ok(res)) {
        nob_log(NOB_ERROR, "failed to create compute pipeline");
        nob_return_defer(false);
    }

defer:
    vkDestroyShaderModule(ctx.device, shader_ci.module, NULL);
    return result;
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

    return vk_ok(vkCreateRenderPass(ctx.device, &render_pass_ci, NULL, &ctx.render_pass));
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
        if (!vk_ok(vkCreateFramebuffer(ctx.device, &frame_buff_ci, NULL, &ctx.swapchain.buffs.items[i])))
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
    vkCmdBeginRenderPass(cmd_man.buff, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
}

bool vk_draw(Pipeline_Type pipeline_type, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix mvp)
{
    bool result = true;

    if (pipeline_type == PIPELINE_POINT_CLOUD || pipeline_type == PIPELINE_POINT_CLOUD_ADV) {
        nob_log(NOB_ERROR, "point cloud pipelines should use vk_draw_points");
        nob_return_defer(false);
    }

    VkCommandBuffer cmd_buffer = cmd_man.buff;
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

bool vk_draw_points(Vk_Buffer vtx_buff, Matrix mvp, Example example)
{
    bool result = true;

    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    if (example == EXAMPLE_ADV_POINT_CLOUD) {
        pipeline = ctx.pipelines[PIPELINE_POINT_CLOUD_ADV];
        pipeline_layout = ctx.pipeline_layouts[PIPELINE_POINT_CLOUD_ADV];
    } else {
        pipeline = ctx.pipelines[PIPELINE_POINT_CLOUD];
        pipeline_layout = ctx.pipeline_layouts[PIPELINE_POINT_CLOUD];
    }

    VkCommandBuffer cmd_buffer = cmd_man.buff;
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
        VkDescriptorSet *descriptor_set = &ctx.ubos[UBO_TYPE_ADV_POINT_CLOUD].descriptor_set;
        vkCmdBindDescriptorSets(
                cmd_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                ctx.pipeline_layouts[PIPELINE_POINT_CLOUD_ADV], 0, 1, descriptor_set, 0, NULL
                );

        vkCmdBindDescriptorSets(
                cmd_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                ctx.pipeline_layouts[PIPELINE_POINT_CLOUD_ADV], 1, 1,
                &ctx.texture_sets[SET_LAYOUT_ADV_POINT_CLOUD_SAMPLER].descriptor_set, 0, NULL);
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

    VkCommandBuffer cmd_buffer = cmd_man.buff;

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
        &ctx.ubos[UBO_TYPE_TEX].descriptor_set, 0, NULL
    );

    Vk_Texture_Set texture_set = ctx.texture_sets[SET_LAYOUT_TEX_SAMPLER];
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
    bool result = true;

    VkResult vk_result = vkWaitForFences(
        ctx.device, 1, &cmd_man.fence, VK_TRUE, UINT64_MAX
    );
    vk_chk(vk_result, "failed to wait for fences");

    vk_result = vkAcquireNextImageKHR(ctx.device,
        ctx.swapchain.handle,
        UINT64_MAX,
        cmd_man.img_avail_sem,
        VK_NULL_HANDLE,
        &img_idx
    );

    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        cvr_chk(vk_swapchain_init(), "failed to recreate swapchain");
    } else if (!vk_ok(vk_result) && vk_result != VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_ERROR, "failed to acquire swapchain image");
        nob_return_defer(false);
    } else if (vk_result == VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_WARNING, "suboptimal swapchain image");
    }

    vk_chk(vkResetFences(ctx.device, 1, &cmd_man.fence), "failed to reset fences");
    vk_chk(vkResetCommandBuffer(cmd_man.buff, 0), "failed to reset cmd buffer");

    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_chk(vkBeginCommandBuffer(cmd_man.buff, &beginInfo), "failed to begin command buffer");

defer:
    return result;
}

bool vk_end_drawing()
{
    bool result = true;

    VkCommandBuffer cmd_buffer = cmd_man.buff;
    vkCmdEndRenderPass(cmd_buffer);
    vk_chk(vkEndCommandBuffer(cmd_buffer), "failed to record command buffer");

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &cmd_man.img_avail_sem;
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_man.buff;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &cmd_man.render_fin_sem;

    vk_chk(vkQueueSubmit(ctx.gfx_queue, 1, &submit, cmd_man.fence), "failed to submit command");

    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &cmd_man.render_fin_sem;
    present.swapchainCount = 1;
    present.pSwapchains = &ctx.swapchain.handle;
    present.pImageIndices = &img_idx;

    VkResult vk_result = vkQueuePresentKHR(ctx.present_queue, &present);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR || ctx.swapchain.buff_resized) {
        ctx.swapchain.buff_resized = false;
        cvr_chk(vk_recreate_swapchain(), "failed to recreate swapchain");
    } else if (!vk_ok(vk_result)) {
        nob_log(NOB_ERROR, "failed to present queue");
        nob_return_defer(false);
    }

defer:
    return result;
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

bool vk_ubo_init(Vk_Buffer *buff)
{
    bool result = true;

    if (buff->size == 0) {
        nob_log(NOB_ERROR, "must specify ubo size");
        nob_return_defer(false);
    }
    result = vk_buff_init(
        buff,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (!result) nob_return_defer(false);
    vkMapMemory(ctx.device, buff->mem, 0, buff->size, 0, &buff->mapped);

defer:
    return result;
}

bool vk_ubo_descriptor_set_layout_init(VkShaderStageFlags flags, uint32_t binding, VkDescriptorSetLayout *layout)
{
    bool result = true;

    VkDescriptorSetLayoutBinding set_layout_binding = {
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = flags,
    };

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &set_layout_binding,
    };

    if (!vk_ok(vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, layout))) {
        nob_log(NOB_ERROR, "failed to create descriptor set layout for uniform buffer");
        nob_return_defer(false);
    }

defer:
    return result;
}

typedef struct {
    VkDescriptorSetLayoutBinding *items;
    size_t count;
    size_t capacity;
} Descriptor_Set_Layout_Bindings;

bool vk_sampler_descriptor_set_layout_init(Set_Layout_Type layout_type)
{
    bool result = true;

    VkDescriptorSetLayoutBinding set_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &set_layout_binding,
    };

    /* handle the advanced point cloud */
    Descriptor_Set_Layout_Bindings bindings = {0};
    Vk_Texture_Set texture_set = ctx.texture_sets[layout_type];
    if (layout_type == SET_LAYOUT_ADV_POINT_CLOUD_SAMPLER) {
        for (size_t i = 0; i < texture_set.count; i++) {
            VkDescriptorSetLayoutBinding binding = {
                .binding = i,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            };
            nob_da_append(&bindings, binding);
        }
        layout_ci.pBindings = bindings.items;
        layout_ci.bindingCount = bindings.count;
    }

    VkDescriptorSetLayout *layout = &ctx.texture_sets[layout_type].set_layout;
    if (!vk_ok(vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, layout))) {
        nob_log(NOB_ERROR, "failed to create descriptor set layout for texture");
        nob_return_defer(false);
    }

defer:
    return result;
}

bool vk_ubo_descriptor_set_init(UBO *ubo)
{
    bool result = true;

    /* allocate uniform buffer descriptor sets */
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

bool vk_sampler_descriptor_pool_init(Vk_Texture_Set *texture_set)
{
    bool result = true;

    uint32_t max_sets = 0;
    VkDescriptorPoolSize pool_size = {0};
    if (texture_set->count) {
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = texture_set->count;
        max_sets += texture_set->count;
    } else {
        nob_log(NOB_ERROR, "no textures loaded, descriptor pool failure");
        nob_return_defer(false);
    }

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = max_sets,
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

bool vk_ubo_descriptor_pool(VkDescriptorPool *pool)
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

    VkResult res = vkCreateDescriptorPool(ctx.device, &pool_ci, NULL, pool);
    if(!VK_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "failed to create descriptor pool");
        nob_return_defer(false);
    }

defer:
    return result;
}

bool vk_sampler_descriptor_set_init(Vk_Texture_Set *texture_set, bool group_textures)
{
    bool result = true;

    /* allocate texture descriptor sets */
    VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = texture_set->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &texture_set->set_layout,
    };

    if (group_textures) {
        VkDescriptorSet *set = &texture_set->descriptor_set;
        VkResult res = vkAllocateDescriptorSets(ctx.device, &alloc, set);
        if (!VK_SUCCEEDED(res)) {
            nob_log(NOB_ERROR, "failed to allocate texture descriptor set");
            goto defer;
        }
    } else {
        for (size_t i = 0; i < texture_set->count; i++) {
            VkDescriptorSet *set = &texture_set->items[i].descriptor_set;
            VkResult res = vkAllocateDescriptorSets(ctx.device, &alloc, set);
            if (!VK_SUCCEEDED(res)) {
                nob_log(NOB_ERROR, "failed to allocate texture descriptor set");
                goto defer;
            }
        }
    }

    /* update texture descriptor sets */
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };

    for (size_t tex = 0; tex < texture_set->count; tex++) {
        VkDescriptorImageInfo img_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView   = texture_set->items[tex].view,
            .sampler     = texture_set->items[tex].sampler,
        };
        write.pImageInfo = &img_info;
        if (group_textures) {
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
        return vk_ok(vkCreateDebugUtilsMessengerEXT(ctx.instance, &debug_msgr_ci, NULL, &ctx.debug_msgr));
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

void frame_buff_resized(GLFWwindow* window, int width, int height)
{
    (void)window;
    (void)width;
    (void)height;
    ctx.swapchain.buff_resized = true;
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
        desc.format = VK_FORMAT_R8G8B8_UINT;
        desc.offset = 12;
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
    bool result = true;

    result = vk_buff_init(
        vtx_buff,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create vertex buffer");

    vk_chk(vkMapMemory(ctx.device, vtx_buff->mem, 0, vtx_buff->size, 0, &(vtx_buff->mapped)), "failed to map memory");
    memcpy(vtx_buff->mapped, data, vtx_buff->size);
    vkUnmapMemory(ctx.device, vtx_buff->mem);

defer:
    return result;
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
    bool result = true;

    result = vk_buff_init(
        idx_buff,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create index buffer");

    vk_chk(vkMapMemory(ctx.device, idx_buff->mem, 0, idx_buff->size, 0, &(idx_buff->mapped)), "failed to map memory");
    memcpy(idx_buff->mapped, data, idx_buff->size);
    vkUnmapMemory(ctx.device, idx_buff->mem);

defer:
    return result;
}


bool vk_buff_copy(Vk_Buffer dst_buff, Vk_Buffer src_buff, VkDeviceSize size)
{
    bool result = true;

    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(&tmp_cmd_buff), "failed quick cmd begin");

    VkBufferCopy copy_region = {0};
    if (size) {
        copy_region.size = size;
        cvr_chk(size <= dst_buff.size, "Cannot copy buffer, size > dst buffer (won't fit)");
        cvr_chk(size <= src_buff.size, "Cannot copy buffer, size > src buffer (cannot copy more than what's available)");
    } else {
        cvr_chk(dst_buff.size >= src_buff.size, "Cannot copy buffer, dst buffer < src buffer (won't fit)");
        copy_region.size = src_buff.size;
    }

    vkCmdCopyBuffer(tmp_cmd_buff, src_buff.handle, dst_buff.handle, 1, &copy_region);

    cvr_chk(cmd_quick_end(&tmp_cmd_buff), "failed quick cmd end");

defer:
    return result;
}

bool vk_img_init(Vk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    bool result = true;

    cvr_chk(img->extent.width, "Vk_Image must be set with width before calling vk_img_init");
    cvr_chk(img->extent.height, "Vk_Image must be set with height before calling vk_img_init");

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

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(ctx.device, &alloc_ci, NULL, &img->mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");

    vk_chk(vkBindImageMemory(ctx.device, img->handle, img->mem, 0), "failed to bind image buffer memory");

defer:
    return result;
}

bool cmd_man_init()
{
    bool result = true;
    cvr_chk(cmd_pool_create(), "failed to create command pool");
    cvr_chk(cmd_buff_create(), "failed to create cmd buffers");
    cvr_chk(cmd_syncs_create(), "failed to create cmd sync objects");

defer:
    return result;
}

bool cmd_pool_create()
{
    bool result = true;
    Queue_Fams queue_fams = find_queue_fams(ctx.phys_device);
    cvr_chk(queue_fams.has_gfx, "failed to create command pool, no graphics queue");
    VkCommandPoolCreateInfo cmd_pool_ci = {0};
    cmd_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_ci.queueFamilyIndex = queue_fams.gfx_idx;
    vk_chk(vkCreateCommandPool(ctx.device, &cmd_pool_ci, NULL, &cmd_man.pool), "failed to create command pool");

defer:
    return result;
}

bool cmd_buff_create()
{
    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.commandPool = cmd_man.pool;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ci.commandBufferCount = 1;
    return vk_ok(vkAllocateCommandBuffers(ctx.device, &ci, &cmd_man.buff));
}

void cmd_man_destroy()
{
    vkDestroySemaphore(ctx.device, cmd_man.img_avail_sem, NULL);
    vkDestroySemaphore(ctx.device, cmd_man.render_fin_sem, NULL);
    vkDestroyFence(ctx.device, cmd_man.fence, NULL);
    vkDestroyCommandPool(ctx.device, cmd_man.pool, NULL);
}

bool cmd_syncs_create()
{
    bool result = true;
    VkSemaphoreCreateInfo sem_ci = {0};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci = {0};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkResult vk_result;
    vk_result = vkCreateSemaphore(ctx.device, &sem_ci, NULL, &cmd_man.img_avail_sem);
    vk_chk(vk_result, "failed to create semaphore");
    vk_result = vkCreateSemaphore(ctx.device, &sem_ci, NULL, &cmd_man.render_fin_sem);
    vk_chk(vk_result, "failed to create semaphore");
    vk_result = vkCreateFence(ctx.device, &fence_ci, NULL, &cmd_man.fence);
    vk_chk(vk_result, "failed to create fence");

defer:
    return result;
}

bool cmd_quick_begin(VkCommandBuffer *tmp_cmd_buff)
{
    bool result = true;

    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ci.commandPool = cmd_man.pool;
    ci.commandBufferCount = 1;
    vk_chk(vkAllocateCommandBuffers(ctx.device, &ci, tmp_cmd_buff), "failed to create quick cmd");

    VkCommandBufferBeginInfo cmd_begin = {0};
    cmd_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_chk(vkBeginCommandBuffer(*tmp_cmd_buff, &cmd_begin), "failed to begin quick cmd");

defer:
    return result;
}

bool cmd_quick_end(VkCommandBuffer *tmp_cmd_buff)
{
    bool result = true;
    vk_chk(vkEndCommandBuffer(*tmp_cmd_buff), "failed to end cmd buffer");

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = tmp_cmd_buff;
    vk_chk(vkQueueSubmit(ctx.gfx_queue, 1, &submit, VK_NULL_HANDLE), "failed to submit quick cmd");
    vk_chk(vkQueueWaitIdle(ctx.gfx_queue), "failed to wait idle in quick cmd");

defer:
    vkFreeCommandBuffers(ctx.device, cmd_man.pool, 1, tmp_cmd_buff);
    return result;
}

bool transition_img_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    bool result = true;
    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(&tmp_cmd_buff), "failed quick cmd begin");
    
    bool layout_dst = old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
                      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bool layout_shader = old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                         new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const char *msg = nob_temp_sprintf(
        "old_layout %d with new_layout %d not allowed yet",
        old_layout,
        new_layout
    );
    assert(layout_dst || (layout_shader && msg));

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = (layout_dst) ? 0 : VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = (layout_dst) ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
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
        tmp_cmd_buff,
        (layout_dst) ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
        (layout_dst) ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
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

bool vk_load_texture(void *data, size_t width, size_t height, VkFormat fmt, size_t *id, bool pc_texture)
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
        goto defer;
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

    Vk_Texture_Set *texture_set = NULL;
    if (pc_texture)
        texture_set = &ctx.texture_sets[SET_LAYOUT_ADV_POINT_CLOUD_SAMPLER];
    else
        texture_set = &ctx.texture_sets[SET_LAYOUT_TEX_SAMPLER];
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

bool vk_unload_texture(size_t id)
{
    vkDeviceWaitIdle(ctx.device);

    Vk_Texture_Set texture_set = ctx.texture_sets[SET_LAYOUT_TEX_SAMPLER];
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

bool vk_unload_pc_texture(size_t id)
{
    vkDeviceWaitIdle(ctx.device); // TODO: alternatives?

    Vk_Texture_Set texture_set = ctx.texture_sets[SET_LAYOUT_ADV_POINT_CLOUD_SAMPLER];
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

#endif // VK_CTX_IMPLEMENTATION
