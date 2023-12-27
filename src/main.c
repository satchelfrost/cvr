#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

// unofficial nob
#define nob_da_resize(da, new_size)                                                    \
    do {                                                                               \
        (da)->capacity = (da)->count = new_size;                                       \
        (da)->items = NOB_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items)); \
    } while (0)

#define LOAD_PFN(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(app.instance, #pfn)
#define UNUSED(x) (void)(x)
#define MIN_SEVERITY NOB_WARNING
#define VK_OK(x) ((x) == VK_SUCCESS)
#define CVR_CHK(expr, msg)           \
    do {                             \
        if (!(expr)) {               \
            nob_log(NOB_ERROR, msg); \
            nob_return_defer(false); \
        }                            \
    } while (0)
#define VK_CHK(vk_result, msg) CVR_CHK(VK_OK(vk_result), msg)
#define Vec(type) struct { \
    type *items;           \
    size_t capacity;       \
    size_t count;          \
}
#define CLAMP(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))

typedef struct {
    uint32_t gfx_idx;
    bool has_gfx;
    uint32_t present_idx;
    bool has_present;
} QueueFamilyIndices;

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

// globals
App app = {0};
static const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
static const char *device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
static const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
static const size_t MAX_FRAMES_IN_FLIGHT = 2;
uint32_t curr_frame = 0;

bool init_window();
bool init_vulkan();
bool main_loop();
bool cleanup();
bool create_instance();
bool chk_validation_support();
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data);
Nob_Log_Level translate_msg_severity(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity);
bool setup_debug_msgr();
void populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *debug_msgr_ci);
typedef Vec(const char*) RequiredExts;
bool inst_exts_satisfied(RequiredExts req_exts);
bool pick_phys_device();
bool is_device_suitable(VkPhysicalDevice phys_device);
QueueFamilyIndices find_queue_fams(VkPhysicalDevice phys_device);
bool create_device();
bool create_surface();
typedef Vec(uint32_t) U32_Set;
void populate_set(int arr[], size_t arr_size, U32_Set *set);
bool create_swpchain();
bool device_exts_supported(VkPhysicalDevice phys_device);
bool swpchain_adequate(VkPhysicalDevice phys_device);
VkSurfaceFormatKHR choose_swpchain_fmt();
VkPresentModeKHR choose_present_mode();
VkExtent2D choose_swp_extent();
bool create_img_views();
bool create_gfx_pipeline();
bool create_shader_module(const char *shader, VkShaderModule *module);
bool create_render_pass();
bool create_frame_buffs();
bool create_cmd_pool();
bool create_cmd_buff();
bool create_syncs();
bool draw_frame();
bool rec_cmds(uint32_t img_idx, VkCommandBuffer cmd_buffer);

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
        CVR_CHK(draw_frame(), "failed to draw frame");
    }

defer:
    vkDeviceWaitIdle(app.device);
    return result;
}

bool init_vulkan()
{
    bool result = true;
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
    return true;
}

bool create_instance()
{
    bool result = true;
#ifdef ENABLE_VALIDATION
    CVR_CHK(chk_validation_support(), "validation requested, but not supported");
#endif

    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "C + Vulkan = Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_ci = {0};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;
    uint32_t glfw_ext_count = 0;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    RequiredExts req_exts = {0};
    for (size_t i = 0; i < glfw_ext_count; i++)
        nob_da_append(&req_exts, glfw_exts[i]);
#ifdef ENABLE_VALIDATION
    instance_ci.enabledLayerCount = NOB_ARRAY_LEN(validation_layers);
    instance_ci.ppEnabledLayerNames = validation_layers;
    nob_da_append(&req_exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
    populated_debug_msgr_ci(&debug_msgr_ci);
    instance_ci.pNext = &debug_msgr_ci;
#endif
    instance_ci.enabledExtensionCount = req_exts.count;
    instance_ci.ppEnabledExtensionNames = req_exts.items;

    CVR_CHK(inst_exts_satisfied(req_exts), "unsatisfied instance extensions");
    VK_CHK(vkCreateInstance(&instance_ci, NULL, &app.instance), "failed to create vulkan instance");

defer:
    nob_da_free(req_exts);
    return result;
}


bool chk_validation_support()
{
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties avail_layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, avail_layers);
    size_t unsatisfied_layers = NOB_ARRAY_LEN(validation_layers);
    for (size_t i = 0; i < NOB_ARRAY_LEN(validation_layers); i++) {
        for (size_t j = 0; j < layer_count; j++) {
            if (strcmp(validation_layers[i], avail_layers[j].layerName) == 0) {
                if (--unsatisfied_layers == 0)
                    return true;
            }
        }
    }

    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data)
{
    UNUSED(msg_type);
    UNUSED(p_user_data);
    Nob_Log_Level log_lvl = translate_msg_severity(msg_severity);
    if (log_lvl < MIN_SEVERITY) return VK_FALSE;
    nob_log(log_lvl, p_callback_data->pMessage);
    return VK_FALSE;
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
    LOAD_PFN(vkCreateDebugUtilsMessengerEXT);
    if (vkCreateDebugUtilsMessengerEXT) {
        return VK_OK(vkCreateDebugUtilsMessengerEXT(app.instance, &debug_msgr_ci, NULL, &app.debug_msgr));
    } else {
        nob_log(NOB_ERROR, "failed to load function pointer for vkCreateDebugUtilesMessenger");
        return false;
    }
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

bool inst_exts_satisfied(RequiredExts req_exts)
{
    uint32_t avail_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, avail_exts);
    size_t unsatisfied_exts = req_exts.count;
    for (size_t i = 0; i < req_exts.count; i++) {
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(req_exts.items[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
            }
        }
    }

    return false;
}

bool pick_phys_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(app.instance, &device_count, NULL);
    VkPhysicalDevice phys_devices[device_count];
    vkEnumeratePhysicalDevices(app.instance, &device_count, phys_devices);
    for (size_t i = 0; i < device_count; i++) {
        if (is_device_suitable(phys_devices[i])) {
            app.phys_device = phys_devices[i];
            return true;
        }
    }

    return false;
}

bool is_device_suitable(VkPhysicalDevice phys_device)
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(phys_device);
    CVR_CHK(indices.has_gfx && indices.has_present, "requested indices not present");
    CVR_CHK(device_exts_supported(phys_device), "device extensions not supported");
    CVR_CHK(swpchain_adequate(phys_device), "swapchain was not adequate");

defer:
    return result;
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
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, app.surface, &present_support);
        if (present_support) {
            indices.present_idx = i;
            indices.has_present = true;
        }

        if (indices.has_gfx && indices.has_present)
            return indices;
    }
    return indices;
}

bool create_device()
{
    QueueFamilyIndices indices = find_queue_fams(app.phys_device);

    int queue_fams[] = {indices.gfx_idx, indices.present_idx};
    U32_Set unique_fams = {0};
    populate_set(queue_fams, NOB_ARRAY_LEN(queue_fams), &unique_fams);

    Vec(VkDeviceQueueCreateInfo) queue_cis = {0};
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
    device_ci.pEnabledFeatures = &features;
    device_ci.pQueueCreateInfos = queue_cis.items;
    device_ci.queueCreateInfoCount = queue_cis.count;
    device_ci.enabledExtensionCount = NOB_ARRAY_LEN(device_exts);
    device_ci.ppEnabledExtensionNames = device_exts;
#ifdef ENABLE_VALIDATION
    device_ci.enabledLayerCount = NOB_ARRAY_LEN(validation_layers);
    device_ci.ppEnabledLayerNames = validation_layers;
#endif

    bool result = true;
    if (VK_OK(vkCreateDevice(app.phys_device, &device_ci, NULL, &app.device))) {
        vkGetDeviceQueue(app.device, indices.gfx_idx, 0, &app.gfx_queue);
        vkGetDeviceQueue(app.device, indices.present_idx, 0, &app.present_queue);
    } else {
        nob_return_defer(false);
    }

defer:
    nob_da_free(queue_cis);
    return result;
}

bool create_surface()
{
    return VK_OK(glfwCreateWindowSurface(app.instance, app.window, NULL, &app.surface));
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

bool create_swpchain()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app.phys_device, app.surface, &capabilities);
    app.surface_fmt = choose_swpchain_fmt();
    uint32_t img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && img_count > capabilities.maxImageCount)
        img_count = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swpchain_ci = {0};
    swpchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swpchain_ci.surface = app.surface;
    swpchain_ci.minImageCount = img_count;
    swpchain_ci.imageFormat = app.surface_fmt.format;
    swpchain_ci.imageColorSpace = app.surface_fmt.colorSpace;
    swpchain_ci.imageExtent = app.extent = choose_swp_extent();
    swpchain_ci.imageArrayLayers = 1;
    swpchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    QueueFamilyIndices indices = find_queue_fams(app.phys_device);
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

    if (VK_OK(vkCreateSwapchainKHR(app.device, &swpchain_ci, NULL, &app.swpchain))) {
        uint32_t img_count = 0;
        vkGetSwapchainImagesKHR(app.device, app.swpchain, &img_count, NULL);
        nob_da_resize(&app.swpchain_imgs, img_count);
        vkGetSwapchainImagesKHR(app.device, app.swpchain, &img_count, app.swpchain_imgs.items);
        return true;
    } else {
        return false;
    }
}

bool device_exts_supported(VkPhysicalDevice phys_device)
{
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &ext_count, NULL);
    VkExtensionProperties avail_exts[ext_count];
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &ext_count, avail_exts);
    uint32_t unsatisfied_exts = NOB_ARRAY_LEN(device_exts);
    for (size_t i = 0; i < NOB_ARRAY_LEN(device_exts); i++) {
        for (size_t j = 0; j < ext_count; j++) {
            if (strcmp(device_exts[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
            }
        }
    }

    return false;
}

bool swpchain_adequate(VkPhysicalDevice phys_device)
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, app.surface, &surface_fmt_count, NULL);
    if (!surface_fmt_count) return false;
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, app.surface, &present_mode_count, NULL);
    if (!present_mode_count) return false;

    return true;
}

VkSurfaceFormatKHR choose_swpchain_fmt()
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(app.phys_device, app.surface, &surface_fmt_count, NULL);
    VkSurfaceFormatKHR fmts[surface_fmt_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(app.phys_device, app.surface, &surface_fmt_count, fmts);
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
    vkGetPhysicalDeviceSurfacePresentModesKHR(app.phys_device, app.surface, &present_mode_count, NULL);
    VkPresentModeKHR present_modes[present_mode_count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(app.phys_device, app.surface, &present_mode_count, present_modes);
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app.phys_device, app.surface, &capabilities);
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(app.window, &width, &height);

        VkExtent2D extent = {
            .width = width,
            .height = height
        };

        extent.width = CLAMP(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = CLAMP(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

bool create_img_views()
{
    nob_da_resize(&app.swpchain_img_views, app.swpchain_imgs.count);
    for (size_t i = 0; i < app.swpchain_img_views.count; i++)  {
        VkImageViewCreateInfo img_view_ci = {0};
        img_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        img_view_ci.image = app.swpchain_imgs.items[i];
        img_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        img_view_ci.format = app.surface_fmt.format;
        img_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_view_ci.subresourceRange.baseMipLevel = 0;
        img_view_ci.subresourceRange.levelCount = 1;
        img_view_ci.subresourceRange.baseArrayLayer = 0;
        img_view_ci.subresourceRange.layerCount = 1;
        if (!VK_OK(vkCreateImageView(app.device, &img_view_ci, NULL, &app.swpchain_img_views.items[i])))
            return false;
    }

    return true;
}

bool create_gfx_pipeline()
{
    bool result = true;
    VkPipelineShaderStageCreateInfo vert_ci = {0};
    vert_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_ci.pName = "main";
    if (!create_shader_module("./build/shaders/shader.vert.spv", &vert_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo frag_ci = {0};
    frag_ci .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_ci .stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_ci.pName = "main";
    if (!create_shader_module("./build/shaders/shader.frag.spv", &frag_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo stages[] = {vert_ci, frag_ci};

    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {0};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_ci.dynamicStateCount = NOB_ARRAY_LEN(dynamic_states);
    dynamic_state_ci.pDynamicStates = dynamic_states;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {0};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {0};
    input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0};
    viewport.width = (float) app.extent.width;
    viewport.height = (float) app.extent.height;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {0};
    scissor.extent = app.extent;
    VkPipelineViewportStateCreateInfo viewport_state_ci = {0};
    viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.viewportCount = 1;
    viewport_state_ci.pViewports = &viewport;
    viewport_state_ci.scissorCount = 1;
    viewport_state_ci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {0};
    rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_ci.lineWidth = 1.0f;

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
    VkResult vk_result = vkCreatePipelineLayout(
        app.device,
        &pipeline_layout_ci,
        NULL,
        &app.pipeline_layout
    );
    VK_CHK(vk_result, "failed to create pipeline layout");

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
    pipeline_ci.layout = app.pipeline_layout;
    pipeline_ci.renderPass = app.render_pass;
    pipeline_ci.subpass = 0;

    vk_result = vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &app.pipeline);
    VK_CHK(vk_result, "failed to create pipeline");

defer:
    vkDestroyShaderModule(app.device, frag_ci.module, NULL);
    vkDestroyShaderModule(app.device, vert_ci.module, NULL);
    return result;
}

bool create_shader_module(const char *shader, VkShaderModule *module)
{
    bool result = true;
    Nob_String_Builder sb = {};
    char *err_msg = nob_temp_sprintf("failed to read entire file %s", shader);
    CVR_CHK(nob_read_entire_file(shader, &sb), err_msg);

    VkShaderModuleCreateInfo module_ci = {0};
    module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_ci.codeSize = sb.count;
    module_ci.pCode = (const uint32_t *)sb.items;
    err_msg = nob_temp_sprintf("failed to create shader module from %s", shader);
    VK_CHK(vkCreateShaderModule(app.device, &module_ci, NULL, module), err_msg);

defer:
    nob_sb_free(sb);
    return result;
}

bool create_render_pass()
{
    VkAttachmentDescription color_attach = {0};
    color_attach.format = app.surface_fmt.format;
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

    return VK_OK(vkCreateRenderPass(app.device, &render_pass_ci, NULL, &app.render_pass));
}

bool create_frame_buffs()
{
    nob_da_resize(&app.frame_buffs, app.swpchain_img_views.count);
    for (size_t i = 0; i < app.swpchain_img_views.count; i++) {
        VkFramebufferCreateInfo frame_buff_ci = {0};
        frame_buff_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buff_ci.renderPass = app.render_pass;
        frame_buff_ci.attachmentCount = 1;
        frame_buff_ci.pAttachments = &app.swpchain_img_views.items[i];
        frame_buff_ci.width =  app.extent.width;
        frame_buff_ci.height = app.extent.height;
        frame_buff_ci.layers = 1;
        if (!VK_OK(vkCreateFramebuffer(app.device, &frame_buff_ci, NULL, &app.frame_buffs.items[i])))
            return false;
    }

    return true;
}

bool create_cmd_pool()
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(app.phys_device);
    CVR_CHK(indices.has_gfx, "failed to create command pool, no graphics queue");
    VkCommandPoolCreateInfo cmd_pool_ci = {0};
    cmd_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_ci.queueFamilyIndex = indices.gfx_idx;
    VK_CHK(vkCreateCommandPool(app.device, &cmd_pool_ci, NULL, &app.cmd_pool), "failed to create command pool");

defer:
    return result;
}

bool create_cmd_buff()
{
    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.commandPool = app.cmd_pool;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    nob_da_resize(&app.cmd_buffers, MAX_FRAMES_IN_FLIGHT);
    ci.commandBufferCount = app.cmd_buffers.count;
    return VK_OK(vkAllocateCommandBuffers(app.device, &ci, app.cmd_buffers.items));
}

bool create_syncs()
{
    bool result = true;
    VkSemaphoreCreateInfo sem_ci = {0};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci = {0};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    nob_da_resize(&app.img_available_sems, MAX_FRAMES_IN_FLIGHT);
    nob_da_resize(&app.render_finished_sems, MAX_FRAMES_IN_FLIGHT);
    nob_da_resize(&app.fences, MAX_FRAMES_IN_FLIGHT);
    VkResult vk_result;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk_result = vkCreateSemaphore(app.device, &sem_ci, NULL, &app.img_available_sems.items[i]);
        VK_CHK(vk_result, "failed to create semaphore");
        vk_result = vkCreateSemaphore(app.device, &sem_ci, NULL, &app.render_finished_sems.items[i]);
        VK_CHK(vk_result, "failed to create semaphore");
        vk_result = vkCreateFence(app.device, &fence_ci, NULL, &app.fences.items[i]);
        VK_CHK(vk_result, "failed to create fence");
    }

defer:
    return result;
}

bool draw_frame()
{
    bool result = true;

    VkResult vk_result = vkWaitForFences(app.device, 1, &app.fences.items[curr_frame], VK_TRUE, UINT64_MAX);
    VK_CHK(vk_result, "failed to wait for fences");
    VK_CHK(vkResetFences(app.device, 1, &app.fences.items[curr_frame]), "failed to reset fences");

    uint32_t img_idx = 0;
    vk_result = vkAcquireNextImageKHR(app.device,
        app.swpchain,
        UINT64_MAX,
        app.img_available_sems.items[curr_frame],
        VK_NULL_HANDLE,
        &img_idx
    );
    VK_CHK(vk_result, "failed to acquire next image");

    VK_CHK(vkResetCommandBuffer(app.cmd_buffers.items[curr_frame], 0), "failed to reset cmd buffer");
    rec_cmds(img_idx, app.cmd_buffers.items[curr_frame]);

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &app.img_available_sems.items[curr_frame];
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &app.cmd_buffers.items[curr_frame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &app.render_finished_sems.items[curr_frame];

    VK_CHK(vkQueueSubmit(app.gfx_queue, 1, &submit, app.fences.items[curr_frame]), "failed to draw command buffer");

    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &app.render_finished_sems.items[curr_frame];
    present.swapchainCount = 1;
    present.pSwapchains = &app.swpchain;
    present.pImageIndices = &img_idx;

    VK_CHK(vkQueuePresentKHR(app.present_queue, &present), "failed to present queue");

    curr_frame = (curr_frame + 1) % MAX_FRAMES_IN_FLIGHT;

defer:
    return result;
}

bool rec_cmds(uint32_t img_idx, VkCommandBuffer cmd_buffer)
{
    bool result = true;
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHK(vkBeginCommandBuffer(cmd_buffer, &beginInfo), "failed to begin command buffer");

    VkRenderPassBeginInfo begin_rp = {0};
    begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_rp.renderPass = app.render_pass;
    begin_rp.framebuffer = app.frame_buffs.items[img_idx];
    begin_rp.renderArea.extent = app.extent;
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    begin_rp.clearValueCount = 1;
    begin_rp.pClearValues = &clear_color;
    vkCmdBeginRenderPass(cmd_buffer, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.pipeline);
    VkViewport viewport = {0};
    viewport.width = (float)app.extent.width;
    viewport.height =(float)app.extent.height;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = app.extent;
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd_buffer);
    VK_CHK(vkEndCommandBuffer(cmd_buffer), "failed to record command buffer");

defer:
    return result;
}

int main()
{
    if (!init_window()) return 1;
    if (!init_vulkan()) return 1;
    if (!main_loop()) return 1;
    if (!cleanup()) return 1;

    return 0;
}