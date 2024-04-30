#ifndef VK_CTX_H_
#define VK_CTX_H_

#include "ext/raylib-5.0/raymath.h"
#include <stdint.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>
#include "geometry.h"
#include "ext/nob.h"
#include "nob_ext.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/* Common macro definitions */
#define load_pfn(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(ctx.instance, #pfn)
#define unused(x) (void)(x)
#define MIN_SEVERITY NOB_WARNING
#define vk_ok(x) ((x) == VK_SUCCESS)
#define cvr_chk(expr, msg)           \
    do {                             \
        if (!(expr)) {               \
            nob_log(NOB_ERROR, msg); \
            nob_return_defer(false); \
        }                            \
    } while (0)
#define vk_chk(vk_result, msg) cvr_chk(vk_ok(vk_result), msg)
#define vec(type) struct { \
    type *items;           \
    size_t capacity;       \
    size_t count;          \
}
#define clamp(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))

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

typedef struct {
    VkDevice device;
    size_t size;
    size_t count;
    VkBuffer handle;
    VkDeviceMemory mem;
    void *mapped;
} Vk_Buffer;

typedef struct {
    VkDevice device;
    size_t width;
    size_t height;
    VkImage handle;
    VkDeviceMemory mem;
} Vk_Image;

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
    vec(Vk_Buffer) ubos;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    vec(VkDescriptorSet) descriptor_sets;
    struct {
        VkPipeline dflt;
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
bool create_dflt_pipeline();
bool create_shader_module(const char *file_name, VkShaderModule *module);
bool create_render_pass();
bool create_frame_buffs();
bool recreate_swpchain();

bool create_ubos();
void cvr_update_ubos();
bool create_descriptor_pool();
bool create_descriptor_sets();

/* Manages synchronization info and gets ready for vulkan commands. */
bool begin_draw();

/* Submits vulkan commands. */
bool end_draw();

bool draw(VkPipeline pipline, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix model);

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
void cvr_set_view_proj(Camera camera);

#ifndef CVR_COLOR
typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;
#endif

void cvr_begin_render_pass(Color color);

typedef struct {
    Color clear_color;
    Vector3 cube_color;
    Camera camera;
    VkPrimitiveTopology topology;
    Matrix view;
    Matrix proj;
} Core_State;

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
Core_State core_state = {0};

/* Manage vulkan command and sync info */
typedef struct {
    VkPhysicalDevice phys_device;
    VkDevice device;
    VkCommandPool pool;
    size_t frames_in_flight;
    vec(VkCommandBuffer) buffs;
    vec(VkSemaphore) img_avail_sems;
    vec(VkSemaphore) render_fin_sems;
    vec(VkFence) fences;
} Vk_Cmd_Man;
static Vk_Cmd_Man cmd_man = {0};

/* Initializes a vulkan command manager, must set the physical device & logical device, and frames in flight */
bool cmd_man_init(Vk_Cmd_Man *cmd_man);

/* Destroys a vulkan command manager */
void cmd_man_destroy(Vk_Cmd_Man *cmd_man);

/* Allocates and begins a temporary command buffer. Easy-to-use, not super efficent. */
bool cmd_quick_begin(const Vk_Cmd_Man *cmd_man, VkCommandBuffer *tmp_cmd_buff);

/* Ends and frees a temporary command buffer. Easy-to-use, not super efficent. */
bool cmd_quick_end(const Vk_Cmd_Man *cmd, VkQueue queue, VkCommandBuffer *tmp_cmd_buff);

bool cmd_buff_create(Vk_Cmd_Man *cmd_man);
bool cmd_syncs_create(Vk_Cmd_Man *cmd_man);
bool cmd_pool_create(Vk_Cmd_Man *cmd_man);

/* Manage vulkan extensions*/
typedef vec(const char*) Extensions;
typedef struct {
    Extensions validation_layers;
    Extensions device_exts;
    Extensions inst_exts;
    int inited;
} Ext_Manager;

void init_ext_managner();
void destroy_ext_manager();
bool inst_exts_satisfied();
bool chk_validation_support();
bool device_exts_supported(VkPhysicalDevice phys_device);

/* VK_Buffer must be set with device & size prior to calling this function */
bool vk_buff_init(Vk_Buffer *buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
void vk_buff_destroy(Vk_Buffer buffer);

/* Must first set the size, count, & device */
bool vtx_buff_init(Vk_Buffer *vtx_buff, const void *data);

/* Must first set the size, count, & device */
bool idx_buff_init(Vk_Buffer *idx_buff, const void *data);

/* Copies "size" bytes from src to dst buffer, a value of zero implies copying the whole src buffer */
bool vk_buff_copy(Vk_Buffer dst_buff, Vk_Buffer src_buff, VkDeviceSize size);

/* Vk_Image must be set with device, width, and height */
bool vk_img_init(Vk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

bool vk_img_copy(VkImage dst_img, VkBuffer src_buff, uint32_t width, uint32_t height);

/* Add various static extensions & validation layers here */
static const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
static const char *device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
Ext_Manager ext_manager = {0};

/* Vertex attributes */
VkVertexInputBindingDescription get_binding_desc();
typedef vec(VkVertexInputAttributeDescription) VtxAttrDescs;
void get_attr_descs(VtxAttrDescs *attr_descs);

static const size_t MAX_FRAMES_IN_FLIGHT = 2;
static uint32_t curr_frame = 0;
static uint32_t img_idx = 0;

typedef struct {
    float16 model;
    float16 view;
    float16 proj;
} UBO;

bool cvr_init()
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
    cvr_chk(create_descriptor_set_layout(), "failed to create desciptorset layout");
    cvr_chk(create_frame_buffs(), "failed to create frame buffers");
    cmd_man.phys_device = ctx.phys_device;
    cmd_man.device = ctx.device;
    cmd_man.frames_in_flight = MAX_FRAMES_IN_FLIGHT;
    cvr_chk(cmd_man_init(&cmd_man), "failed to create vulkan command manager");
    cvr_chk(create_ubos(), "failed to create uniform buffer objects");
    cvr_chk(create_descriptor_pool(), "failed to create descriptor pool");
    cvr_chk(create_descriptor_sets(), "failed to create descriptor pool");

    /* Set the default topology to triangle list */
    core_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

defer:
    return result;
}

bool cvr_destroy()
{
    cmd_man_destroy(&cmd_man);
    cleanup_swpchain();
    for (size_t i = 0; i < ctx.ubos.count; i++)
        vk_buff_destroy(ctx.ubos.items[i]);
    nob_da_free(ctx.ubos);
    vkDestroyDescriptorPool(ctx.device, ctx.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, ctx.descriptor_set_layout, NULL);
    nob_da_free(ctx.descriptor_sets);
    vkDestroyPipeline(ctx.device, ctx.pipelines.dflt, NULL);
    ctx.pipelines.dflt = NULL;
    vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layout, NULL);
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

bool create_instance()
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

bool create_device()
{
    QueueFamilyIndices indices = find_queue_fams(ctx.phys_device);

    int queue_fams[] = {indices.gfx_idx, indices.present_idx};
    U32_Set unique_fams = {0};
    populate_set(queue_fams, NOB_ARRAY_LEN(queue_fams), &unique_fams);

    vec(VkDeviceQueueCreateInfo) queue_cis = {0};
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
        vkGetDeviceQueue(ctx.device, indices.gfx_idx, 0, &ctx.gfx_queue);
        vkGetDeviceQueue(ctx.device, indices.present_idx, 0, &ctx.present_queue);
    } else {
        nob_return_defer(false);
    }

defer:
    nob_da_free(queue_cis);
    return result;
}

bool create_surface()
{
    return vk_ok(glfwCreateWindowSurface(ctx.instance, ctx.window, NULL, &ctx.surface));
}

bool create_swpchain()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.phys_device, ctx.surface, &capabilities);
    ctx.surface_fmt = choose_swpchain_fmt();
    uint32_t img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && img_count > capabilities.maxImageCount)
        img_count = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swpchain_ci = {0};
    swpchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swpchain_ci.surface = ctx.surface;
    swpchain_ci.minImageCount = img_count;
    swpchain_ci.imageFormat = ctx.surface_fmt.format;
    swpchain_ci.imageColorSpace = ctx.surface_fmt.colorSpace;
    swpchain_ci.imageExtent = ctx.extent = choose_swp_extent();
    swpchain_ci.imageArrayLayers = 1;
    swpchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    QueueFamilyIndices indices = find_queue_fams(ctx.phys_device);
    uint32_t queue_fams[] = {indices.gfx_idx, indices.present_idx};
    if (indices.gfx_idx != indices.present_idx) {
        swpchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swpchain_ci.queueFamilyIndexCount = 2;
        swpchain_ci.pQueueFamilyIndices = queue_fams;
    } else {
        swpchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    swpchain_ci.clipped = VK_TRUE;
    swpchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swpchain_ci.presentMode = choose_present_mode();
    swpchain_ci.preTransform = capabilities.currentTransform;

    if (vk_ok(vkCreateSwapchainKHR(ctx.device, &swpchain_ci, NULL, &ctx.swpchain.handle))) {
        uint32_t img_count = 0;
        vkGetSwapchainImagesKHR(ctx.device, ctx.swpchain.handle, &img_count, NULL);
        nob_da_resize(&ctx.swpchain.imgs, img_count);
        vkGetSwapchainImagesKHR(ctx.device, ctx.swpchain.handle, &img_count, ctx.swpchain.imgs.items);
        return true;
    } else {
        return false;
    }
}

bool create_img_views()
{
    nob_da_resize(&ctx.swpchain.img_views, ctx.swpchain.imgs.count);
    for (size_t i = 0; i < ctx.swpchain.img_views.count; i++)  {
        VkImageViewCreateInfo img_view_ci = {0};
        img_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        img_view_ci.image = ctx.swpchain.imgs.items[i];
        img_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        img_view_ci.format = ctx.surface_fmt.format;
        img_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_view_ci.subresourceRange.baseMipLevel = 0;
        img_view_ci.subresourceRange.levelCount = 1;
        img_view_ci.subresourceRange.baseArrayLayer = 0;
        img_view_ci.subresourceRange.layerCount = 1;
        if (!vk_ok(vkCreateImageView(ctx.device, &img_view_ci, NULL, &ctx.swpchain.img_views.items[i])))
            return false;
    }

    return true;
}

bool create_dflt_pipeline()
{
    bool result = true;
    VkPipelineShaderStageCreateInfo vert_ci = {0};
    vert_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_ci.pName = "main";
    if (!create_shader_module("./res/shader.vert.spv", &vert_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo frag_ci = {0};
    frag_ci .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_ci .stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_ci.pName = "main";
    if (!create_shader_module("./res/shader.frag.spv", &frag_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo stages[] = {vert_ci, frag_ci};

    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {0};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    dynamic_state_ci.dynamicStateCount = NOB_ARRAY_LEN(dynamic_states);
    dynamic_state_ci.pDynamicStates = dynamic_states;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {0};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VtxAttrDescs vert_attrs = {0};
    get_attr_descs(&vert_attrs);
    VkVertexInputBindingDescription binding_desc = get_binding_desc();
    vertex_input_ci.vertexBindingDescriptionCount = 1;
    vertex_input_ci.pVertexBindingDescriptions = &binding_desc;
    vertex_input_ci.vertexAttributeDescriptionCount = vert_attrs.count;
    vertex_input_ci.pVertexAttributeDescriptions = vert_attrs.items;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {0};
    input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_ci.topology = core_state.topology;

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
    // rasterizer_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_ci.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer_ci.lineWidth = 1.0f;
    // rasterizer_ci.cullMode = VK_CULL_MODE_FRONT_BIT;
    // rasterizer_ci.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_ci.cullMode = VK_CULL_MODE_NONE;
    rasterizer_ci.lineWidth = VK_FRONT_FACE_CLOCKWISE;
    // rasterizer_ci.lineWidth = VK_FRONT_FACE_COUNTER_CLOCKWISE;

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
    pipeline_layout_ci.pSetLayouts = &ctx.descriptor_set_layout;
    pipeline_layout_ci.setLayoutCount = 1;
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
        &ctx.pipeline_layout
    );
    vk_chk(vk_result, "failed to create pipeline layout");

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
    pipeline_ci.layout = ctx.pipeline_layout;
    pipeline_ci.renderPass = ctx.render_pass;
    pipeline_ci.subpass = 0;

    vk_result = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &ctx.pipelines.dflt);
    vk_chk(vk_result, "failed to create pipeline");

defer:
    vkDestroyShaderModule(ctx.device, frag_ci.module, NULL);
    vkDestroyShaderModule(ctx.device, vert_ci.module, NULL);
    nob_da_free(vert_attrs);
    return result;
}

bool create_shader_module(const char *file_name, VkShaderModule *module)
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

bool create_render_pass()
{
    VkAttachmentDescription color_attach = {0};
    color_attach.format = ctx.surface_fmt.format;
    color_attach.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attach.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attach_ref = {0};
    color_attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription gfx_subpass = {0};
    gfx_subpass.colorAttachmentCount = 1;
    gfx_subpass.pColorAttachments = &color_attach_ref;

    VkRenderPassCreateInfo render_pass_ci = {0};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = 1;
    render_pass_ci.pAttachments = &color_attach;
    render_pass_ci.subpassCount = 1;
    render_pass_ci.pSubpasses = &gfx_subpass;
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    render_pass_ci.dependencyCount = 1;
    render_pass_ci.pDependencies = &dependency;

    return vk_ok(vkCreateRenderPass(ctx.device, &render_pass_ci, NULL, &ctx.render_pass));
}

bool create_frame_buffs()
{
    nob_da_resize(&ctx.swpchain.buffs, ctx.swpchain.img_views.count);
    for (size_t i = 0; i < ctx.swpchain.img_views.count; i++) {
        VkFramebufferCreateInfo frame_buff_ci = {0};
        frame_buff_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buff_ci.renderPass = ctx.render_pass;
        frame_buff_ci.attachmentCount = 1;
        frame_buff_ci.pAttachments = &ctx.swpchain.img_views.items[i];
        frame_buff_ci.width =  ctx.extent.width;
        frame_buff_ci.height = ctx.extent.height;
        frame_buff_ci.layers = 1;
        if (!vk_ok(vkCreateFramebuffer(ctx.device, &frame_buff_ci, NULL, &ctx.swpchain.buffs.items[i])))
            return false;
    }

    return true;
}

void cvr_begin_render_pass(Color color)
{
    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];

    VkRenderPassBeginInfo begin_rp = {0};
    begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_rp.renderPass = ctx.render_pass;
    begin_rp.framebuffer = ctx.swpchain.buffs.items[img_idx];
    begin_rp.renderArea.extent = ctx.extent;
    VkClearValue clear_color = {0};
    clear_color.color.float32[0] = color.r / 255.0f;
    clear_color.color.float32[1] = color.g / 255.0f;
    clear_color.color.float32[2] = color.b / 255.0f;
    clear_color.color.float32[3] = color.a / 255.0f;
    begin_rp.clearValueCount = 1;
    begin_rp.pClearValues = &clear_color;
    vkCmdBeginRenderPass(cmd_buffer, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
}

bool draw(VkPipeline pipeline, Vk_Buffer vtx_buff, Vk_Buffer idx_buff, Matrix model)
{
    bool result = true;

    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];

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
    vkCmdBindIndexBuffer(cmd_buffer, idx_buff.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(
        cmd_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ctx.pipeline_layout, 0, 1, &ctx.descriptor_sets.items[curr_frame], 0, NULL
    );

    Matrix viewProj = MatrixMultiply(core_state.view, core_state.proj);
    Matrix mvp = MatrixMultiply(model, viewProj);

    float16 mat = MatrixToFloatV(mvp);
    vkCmdPushConstants(
        cmd_buffer,
        ctx.pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(float16),
        &mat
    );

    vkCmdDrawIndexed(cmd_buffer, idx_buff.count, 1, 0, 0, 0);

    return result;
}

bool begin_draw()
{
    bool result = true;
    VkResult vk_result = vkWaitForFences(ctx.device, 1, &cmd_man.fences.items[curr_frame], VK_TRUE, UINT64_MAX);
    vk_chk(vk_result, "failed to wait for fences");

    vk_result = vkAcquireNextImageKHR(ctx.device,
        ctx.swpchain.handle,
        UINT64_MAX,
        cmd_man.img_avail_sems.items[curr_frame],
        VK_NULL_HANDLE,
        &img_idx
    );
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        cvr_chk(recreate_swpchain(), "failed to recreate swapchain");
    } else if (!vk_ok(vk_result) && vk_result != VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_ERROR, "failed to acquire swapchain image");
        nob_return_defer(false);
    } else if (vk_result == VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_WARNING, "suboptimal swapchain image");
    }

    vk_chk(vkResetFences(ctx.device, 1, &cmd_man.fences.items[curr_frame]), "failed to reset fences");
    vk_chk(vkResetCommandBuffer(cmd_man.buffs.items[curr_frame], 0), "failed to reset cmd buffer");

    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_chk(vkBeginCommandBuffer(cmd_buffer, &beginInfo), "failed to begin command buffer");

defer:
    return result;
}

bool end_draw()
{
    bool result = true;

    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];
    vkCmdEndRenderPass(cmd_buffer);
    vk_chk(vkEndCommandBuffer(cmd_buffer), "failed to record command buffer");

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &cmd_man.img_avail_sems.items[curr_frame];
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_man.buffs.items[curr_frame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &cmd_man.render_fin_sems.items[curr_frame];

    vk_chk(vkQueueSubmit(ctx.gfx_queue, 1, &submit, cmd_man.fences.items[curr_frame]), "failed to submit command");

    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &cmd_man.render_fin_sems.items[curr_frame];
    present.swapchainCount = 1;
    present.pSwapchains = &ctx.swpchain.handle;
    present.pImageIndices = &img_idx;

    VkResult vk_result = vkQueuePresentKHR(ctx.present_queue, &present);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR || ctx.swpchain.buff_resized) {
        ctx.swpchain.buff_resized = false;
        cvr_chk(recreate_swpchain(), "failed to recreate swapchain");
    } else if (!vk_ok(vk_result)) {
        nob_log(NOB_ERROR, "failed to present queue");
        nob_return_defer(false);
    }

    curr_frame = (curr_frame + 1) % MAX_FRAMES_IN_FLIGHT;

defer:
    return result;
}

bool recreate_swpchain()
{
    bool result = true;

    int width = 0, height = 0;
    glfwGetFramebufferSize(ctx.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(ctx.window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx.device);

    cleanup_swpchain();

    cvr_chk(create_swpchain(), "failed to recreate swapchain");
    cvr_chk(create_img_views(), "failed to recreate image views");
    cvr_chk(create_frame_buffs(), "failed to recreate frame buffers");

defer:
    return result;
}

bool create_descriptor_set_layout()
{
    VkDescriptorSetLayoutBinding layout = {0};
    layout.binding = 0;
    layout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layout.descriptorCount = 1;
    layout.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layout_ci = {0};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = 1;
    layout_ci.pBindings = &layout;
    return vk_ok(vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, &ctx.descriptor_set_layout));
}

bool create_ubos()
{
    bool result = true;

    nob_da_resize(&ctx.ubos, MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        Vk_Buffer *buff = &ctx.ubos.items[i];
        buff->size = sizeof(UBO);
        buff->device = ctx.device;
        result = vk_buff_init(
            buff,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        cvr_chk(result, "failed to create uniform buffer");
        vkMapMemory(ctx.device, buff->mem, 0, buff->size, 0, &buff->mapped);
    }

defer:
     return result;
}

void cvr_update_ubos()
{
    UBO ubo = {
        .model = MatrixToFloatV(MatrixIdentity()),
        .view  = MatrixToFloatV(core_state.view),
        .proj  = MatrixToFloatV(core_state.proj),
    };

    memcpy(ctx.ubos.items[curr_frame].mapped, &ubo, sizeof(ubo));
}

bool create_descriptor_pool()
{
    VkDescriptorPoolSize pool_size = {0};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo pool_ci = {0};
    pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.poolSizeCount = 1;
    pool_ci.pPoolSizes = &pool_size;
    pool_ci.maxSets = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    return vk_ok(vkCreateDescriptorPool(ctx.device, &pool_ci, NULL, &ctx.descriptor_pool));
}

bool create_descriptor_sets()
{
    bool result = true;

    vec(VkDescriptorSetLayout) layouts = {0};
    nob_da_resize(&layouts, MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < layouts.count; i++)
        layouts.items[i] = ctx.descriptor_set_layout;

    VkDescriptorSetAllocateInfo alloc = {0};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = ctx.descriptor_pool;
    alloc.descriptorSetCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;
    alloc.pSetLayouts = layouts.items;

    nob_da_resize(&ctx.descriptor_sets, MAX_FRAMES_IN_FLIGHT);
    VkResult vk_result = vkAllocateDescriptorSets(ctx.device, &alloc, ctx.descriptor_sets.items);
    vk_chk(vk_result, "failed to allocate descriptor sets");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buff_info = {0};
        buff_info.buffer = ctx.ubos.items[i].handle;
        buff_info.offset = 0;
        buff_info.range = sizeof(UBO);

        VkWriteDescriptorSet descriptor_write = {0};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = ctx.descriptor_sets.items[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;

        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buff_info;
        vkUpdateDescriptorSets(ctx.device, 1, &descriptor_write, 0, NULL);
    }

defer:
    nob_da_free(layouts);
    return result;
}

bool is_device_suitable(VkPhysicalDevice phys_device)
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(phys_device);
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(phys_device, &props);
    vkGetPhysicalDeviceFeatures(phys_device, &features);
    result &= (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
               props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
    result &= (features.geometryShader);
    cvr_chk(indices.has_gfx && indices.has_present, "requested indices not present");
    cvr_chk(device_exts_supported(phys_device), "device extensions not supported");
    cvr_chk(swpchain_adequate(phys_device), "swapchain was not adequate");

defer:
    return result;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data)
{
    unused(msg_type);
    unused(p_user_data);
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

QueueFamilyIndices find_queue_fams(VkPhysicalDevice phys_device)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fams[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, queue_fams);
    QueueFamilyIndices indices = {0};
    for (size_t i = 0; i < queue_fam_count; i++) {
        if (queue_fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.gfx_idx = i;
            indices.has_gfx = true;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, ctx.surface, &present_support);
        if (present_support) {
            indices.present_idx = i;
            indices.has_present = true;
        }

        if (indices.has_gfx && indices.has_present)
            return indices;
    }
    return indices;
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

bool swpchain_adequate(VkPhysicalDevice phys_device)
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, ctx.surface, &surface_fmt_count, NULL);
    if (!surface_fmt_count) return false;
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, ctx.surface, &present_mode_count, NULL);
    if (!present_mode_count) return false;

    return true;
}

VkSurfaceFormatKHR choose_swpchain_fmt()
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

void cleanup_swpchain()
{
    for (size_t i = 0; i < ctx.swpchain.buffs.count; i++)
        vkDestroyFramebuffer(ctx.device, ctx.swpchain.buffs.items[i], NULL);
    for (size_t i = 0; i < ctx.swpchain.img_views.count; i++)
        vkDestroyImageView(ctx.device, ctx.swpchain.img_views.items[i], NULL);
    vkDestroySwapchainKHR(ctx.device, ctx.swpchain.handle, NULL);

    nob_da_reset(ctx.swpchain.buffs);
    nob_da_reset(ctx.swpchain.img_views);
    nob_da_reset(ctx.swpchain.imgs);
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
    unused(window);
    unused(width);
    unused(height);
    ctx.swpchain.buff_resized = true;
}

void cvr_set_proj(Camera camera)
{
    Matrix proj = {0};
    double aspect = ctx.extent.width / (double) ctx.extent.height;
    double top = camera.fovy / 2.0;
    double right = top * aspect;
    switch (camera.projection) {
    case PERSPECTIVE:
        proj  = MatrixPerspective(camera.fovy * DEG2RAD, aspect, Z_NEAR, Z_FAR);
        break;
    case ORTHOGRAPHIC:
        proj  = MatrixOrtho(-right, right, -top, top, -Z_FAR, Z_FAR);
        break;
    default:
        assert(0 && "unrecognized camera mode");
        break;
    }

    /* Vulkan */
    proj.m5 *= -1.0f;

    core_state.proj = proj;

}

void cvr_set_view(Camera camera)
{
    core_state.view = MatrixLookAt(camera.position, camera.target, camera.up);
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

VkVertexInputBindingDescription get_binding_desc()
{
    VkVertexInputBindingDescription bindingDescription = {0};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

void get_attr_descs(VtxAttrDescs *attr_descs)
{
    VkVertexInputAttributeDescription desc = {0};
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
}

bool vk_buff_init(Vk_Buffer *buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    bool result = true;
    VkBufferCreateInfo buffer_ci = {0};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cvr_chk(buffer->size, "Vk_Buffer must be set with size before calling constructor");
    buffer_ci.size = (VkDeviceSize) buffer->size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    cvr_chk(buffer->device, "Vk_Buffer must be set with device before calling constructor");
    vk_chk(vkCreateBuffer(buffer->device, &buffer_ci, NULL, &buffer->handle), "failed to create buffer");

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(buffer->device, buffer->handle, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(buffer->device, &alloc_ci, NULL, &buffer->mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");

    vk_chk(vkBindBufferMemory(buffer->device, buffer->handle, buffer->mem, 0), "failed to bind buffer memory");

defer:
    return result;
}

void vk_buff_destroy(Vk_Buffer buffer)
{
    if (!buffer.device) {
        nob_log(NOB_WARNING, "cannot destroy null buffer");
        return;
    }
    vkDestroyBuffer(buffer.device, buffer.handle, NULL);
    vkFreeMemory(buffer.device, buffer.mem, NULL);
    buffer.handle = NULL;
}

bool vtx_buff_init(Vk_Buffer *vtx_buff, const void *data)
{
    bool result = true;

    /* create two buffers, a so-called "staging" buffer, and one for our actual vertex buffer */
    Vk_Buffer stg_buff = {
        .device = vtx_buff->device,
        .size   = vtx_buff->size,
        .count  = vtx_buff->count,
    };
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    vk_chk(vkMapMemory(stg_buff.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped), "failed to map memory");
    memcpy(stg_buff.mapped, data, stg_buff.size);
    vkUnmapMemory(stg_buff.device, stg_buff.mem);

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

bool idx_buff_init(Vk_Buffer *idx_buff, const void *data)
{
    bool result = true;

    /* create two buffers, a so-called "staging" buffer, and one for our actual index buffer */
    Vk_Buffer stg_buff = {
        .device = idx_buff->device,
        .size   = idx_buff->size,
        .count  = idx_buff->count,
    };
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    vk_chk(vkMapMemory(stg_buff.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped), "failed to map memory");
    memcpy(stg_buff.mapped, data, stg_buff.size);
    vkUnmapMemory(stg_buff.device, stg_buff.mem);

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


bool vk_buff_copy(Vk_Buffer dst_buff, Vk_Buffer src_buff, VkDeviceSize size)
{
    bool result = true;

    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(&cmd_man, &tmp_cmd_buff), "failed quick cmd begin");

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

    cvr_chk(cmd_quick_end(&cmd_man, ctx.gfx_queue, &tmp_cmd_buff), "failed quick cmd end");

defer:
    return result;
}

bool vk_img_init(Vk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    bool result = true;

    VkImageCreateInfo img_ci = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_R8G8B8A8_SRGB,
        .extent      = {img->width, img->height, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    vk_chk(vkCreateImage(img->device, &img_ci, NULL, &img->handle), "failed to create image");

    VkMemoryRequirements mem_reqs = {0};
    vkGetImageMemoryRequirements(img->device, img->handle, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(img->device, &alloc_ci, NULL, &img->mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");

    vk_chk(vkBindImageMemory(img->device, img->handle, img->mem, 0), "failed to bind buffer memory");

defer:
    return result;
}

bool cmd_man_init(Vk_Cmd_Man *cmd_man)
{
    bool result = true;
    cvr_chk(cmd_pool_create(cmd_man), "failed to create command pool");
    cvr_chk(cmd_buff_create(cmd_man), "failed to create cmd buffers");
    cvr_chk(cmd_syncs_create(cmd_man), "failed to create cmd sync objects");

defer:
    return result;
}

void cmd_man_destroy(Vk_Cmd_Man *cmd_man)
{
    for (size_t i = 0; i < cmd_man->frames_in_flight; i++) {
        vkDestroySemaphore(cmd_man->device, cmd_man->img_avail_sems.items[i], NULL);
        vkDestroySemaphore(cmd_man->device, cmd_man->render_fin_sems.items[i], NULL);
        vkDestroyFence(cmd_man->device, cmd_man->fences.items[i], NULL);
    }
    vkDestroyCommandPool(cmd_man->device, cmd_man->pool, NULL);

    nob_da_reset(cmd_man->buffs);
    nob_da_reset(cmd_man->img_avail_sems);
    nob_da_reset(cmd_man->render_fin_sems);
    nob_da_reset(cmd_man->fences);
}

bool cmd_pool_create(Vk_Cmd_Man *cmd_man)
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(cmd_man->phys_device);
    cvr_chk(indices.has_gfx, "failed to create command pool, no graphics queue");
    VkCommandPoolCreateInfo cmd_pool_ci = {0};
    cmd_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_ci.queueFamilyIndex = indices.gfx_idx;
    vk_chk(vkCreateCommandPool(cmd_man->device, &cmd_pool_ci, NULL, &cmd_man->pool), "failed to create command pool");

defer:
    return result;
}

bool cmd_buff_create(Vk_Cmd_Man *cmd_man)
{
    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.commandPool = cmd_man->pool;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    nob_da_resize(&cmd_man->buffs, cmd_man->frames_in_flight);
    ci.commandBufferCount = cmd_man->buffs.count;
    return vk_ok(vkAllocateCommandBuffers(cmd_man->device, &ci, cmd_man->buffs.items));
}

bool cmd_syncs_create(Vk_Cmd_Man *cmd_man)
{
    bool result = true;
    VkSemaphoreCreateInfo sem_ci = {0};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci = {0};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    size_t num_syncs = cmd_man->frames_in_flight;
    nob_da_resize(&cmd_man->img_avail_sems, num_syncs);
    nob_da_resize(&cmd_man->render_fin_sems, num_syncs);
    nob_da_resize(&cmd_man->fences, num_syncs);
    VkResult vk_result;
    for (size_t i = 0; i < num_syncs; i++) {
        vk_result = vkCreateSemaphore(cmd_man->device, &sem_ci, NULL, &cmd_man->img_avail_sems.items[i]);
        vk_chk(vk_result, "failed to create semaphore");
        vk_result = vkCreateSemaphore(cmd_man->device, &sem_ci, NULL, &cmd_man->render_fin_sems.items[i]);
        vk_chk(vk_result, "failed to create semaphore");
        vk_result = vkCreateFence(cmd_man->device, &fence_ci, NULL, &cmd_man->fences.items[i]);
        vk_chk(vk_result, "failed to create fence");
    }

defer:
    return result;
}

bool cmd_quick_begin(const Vk_Cmd_Man *cmd_man, VkCommandBuffer *tmp_cmd_buff)
{
    bool result = true;

    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ci.commandPool = cmd_man->pool;
    ci.commandBufferCount = 1;
    vk_chk(vkAllocateCommandBuffers(cmd_man->device, &ci, tmp_cmd_buff), "failed to create quick cmd");

    VkCommandBufferBeginInfo cmd_begin = {0};
    cmd_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_chk(vkBeginCommandBuffer(*tmp_cmd_buff, &cmd_begin), "failed to begin quick cmd");

defer:
    return result;
}

bool cmd_quick_end(const Vk_Cmd_Man *cmd_man, VkQueue queue, VkCommandBuffer *tmp_cmd_buff)
{
    bool result = true;
    vk_chk(vkEndCommandBuffer(*tmp_cmd_buff), "failed to end cmd buffer");

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = tmp_cmd_buff;
    vk_chk(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "failed to submit quick cmd");
    vk_chk(vkQueueWaitIdle(queue), "failed to wait idle in quick cmd");

defer:
    vkFreeCommandBuffers(cmd_man->device, cmd_man->pool, 1, tmp_cmd_buff);
    return result;
}

bool transition_img_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    bool result = true;
    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(&cmd_man, &tmp_cmd_buff), "failed quick cmd begin");
    
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

    cvr_chk(cmd_quick_end(&cmd_man, ctx.gfx_queue, &tmp_cmd_buff), "failed quick cmd end");

defer:
    return result;
}

bool vk_img_copy(VkImage dst_img, VkBuffer src_buff, uint32_t width, uint32_t height)
{
    bool result = true;
    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(&cmd_man, &tmp_cmd_buff), "failed quick cmd begin");

    VkBufferImageCopy region = {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };

    vkCmdCopyBufferToImage(
        tmp_cmd_buff,
        src_buff,
        dst_img,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    cvr_chk(cmd_quick_end(&cmd_man, ctx.gfx_queue, &tmp_cmd_buff), "failed quick cmd end");

defer:
    return result;
}

#endif // VK_CTX_IMPLEMENTATION
