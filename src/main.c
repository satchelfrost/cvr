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
#define Vec(type) struct { \
    type *items;           \
    size_t capacity;       \
    size_t count;          \
}
#define CLAMP(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))
#define VK_INIT(func)                           \
    do {                                        \
        if (!func) {                            \
            nob_log(NOB_ERROR, #func" failed"); \
            return false;                       \
        }                                       \
    } while (0)

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
    Vec(VkImage) swpchain_imgs;
    Vec(VkImageView) swpchain_img_views;
} App;

// globals
App app = {0};
const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};
const char *device_exts[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

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
    while(!glfwWindowShouldClose(app.window))
        glfwPollEvents();
    return true;
}

bool init_vulkan()
{
    VK_INIT(create_instance());
#ifdef ENABLE_VALIDATION
    VK_INIT(setup_debug_msgr());
#endif
    VK_INIT(create_surface());
    VK_INIT(pick_phys_device());
    VK_INIT(create_device());
    VK_INIT(create_swpchain());
    VK_INIT(create_img_views());
    VK_INIT(create_gfx_pipeline());

    return true;
}

bool cleanup()
{
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
#ifdef ENABLE_VALIDATION
    if (!chk_validation_support()) {
        nob_log(NOB_ERROR, "validation requested, but not supported");
        return false;
    }
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

    bool result = true;
    if (!inst_exts_satisfied(req_exts)) {
        nob_log(NOB_ERROR, "unsatisfied instance extensions");
        nob_return_defer(false);
    }

    result = VK_OK(vkCreateInstance(&instance_ci, NULL, &app.instance));

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
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        return NOB_INFO;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        return NOB_WARNING;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        return NOB_ERROR;
    default:
        assert(0 && "this message severity is not handled");
    }
}

bool setup_debug_msgr()
{
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
    populated_debug_msgr_ci(&debug_msgr_ci);
    LOAD_PFN(vkCreateDebugUtilsMessengerEXT);
    if (vkCreateDebugUtilsMessengerEXT) {
        VkResult result = vkCreateDebugUtilsMessengerEXT(app.instance, &debug_msgr_ci, NULL, &app.debug_msgr);
        return result == VK_SUCCESS;
    } else {
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
    QueueFamilyIndices indices = find_queue_fams(phys_device);
    if (!(indices.has_gfx && indices.has_present)) {
        nob_log(NOB_ERROR, "requested indices not present");
        return false;
    }
    if (!device_exts_supported(phys_device)) {
        nob_log(NOB_ERROR, "device extensions not supported");
        return false;
    }
    if (!swpchain_adequate(phys_device)) {
        nob_log(NOB_ERROR, "swapchain was not adequate");
        return false;
    }

    return true;
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
    VkPipelineShaderStageCreateInfo vert_ci = {0};
    vert_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_ci.pName = "main";
    if (!create_shader_module("./build/shaders/shader.vert.spv", &vert_ci.module))
        return false;

    VkPipelineShaderStageCreateInfo frag_ci = {0};
    frag_ci .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_ci .stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_ci.pName = "main";
    if (!create_shader_module("./build/shaders/shader.frag.spv", &frag_ci.module))
        return false;

    // VkPipelineShaderStageCreateInfo stages[] = {vert_ci, frag_ci};


    vkDestroyShaderModule(app.device, frag_ci.module, NULL);
    vkDestroyShaderModule(app.device, vert_ci.module, NULL);
    return true;
}

bool create_shader_module(const char *shader, VkShaderModule *module)
{
    bool result = true;
    Nob_String_Builder sb = {};
    if (!nob_read_entire_file(shader, &sb)) {
        nob_log(NOB_ERROR, "failed to read entire file %s", shader);
        nob_return_defer(false);
    }

    VkShaderModuleCreateInfo module_ci = {0};
    module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_ci.codeSize = sb.count;
    module_ci.pCode = (uint32_t *)sb.items;

    if (!VK_OK(vkCreateShaderModule(app.device, &module_ci, NULL, module))) {
        nob_log(NOB_ERROR, "failed to create shader module from %s", shader);
        nob_return_defer(false);
    }

defer:
    nob_sb_free(sb);
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