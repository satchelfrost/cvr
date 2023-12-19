#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

#define LOAD_PFN(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(app.instance, #pfn)
#define UNUSED(x) (void)(x)
#define MIN_SEVERITY NOB_WARNING
#define VK_OK(x) ((x) == VK_SUCCESS)
#define Vec(type) struct { \
    type *items;           \
    size_t capacity;       \
    size_t count;          \
}

typedef struct {
    uint32_t gfx_idx;
    bool has_gfx;
    uint32_t present_idx;
    bool has_present;
} QueueFamilyIndices;

typedef struct {
    const char **items;
    size_t capacity;
    size_t count;
} RequiredExts;

typedef struct {
    GLFWwindow *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_msgr;
    VkPhysicalDevice phys_device;
    VkDevice device;
    VkQueue gfx_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
} App;

// globals
App app = {0};
const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation",
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
void populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *ci);
bool inst_exts_satisfied(RequiredExts req_exts);
bool pick_phys_device();
bool is_device_suitable(VkPhysicalDevice phys_device);
QueueFamilyIndices find_queue_fams(VkPhysicalDevice phys_device);
bool create_device();
bool create_surface();
typedef Vec(uint32_t) U32_Set;
void populate_set(int arr[], size_t arr_size, U32_Set *set);

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
    if (!create_instance()) {
        nob_log(NOB_ERROR, "failed to create vulkan instance");
        return false;
    }

#ifdef ENABLE_VALIDATION
    if (!setup_debug_msgr()) {
        nob_log(NOB_ERROR, "failed to setup debug messenger");
        return false;
    }
#endif

    if (!create_surface()) {
        nob_log(NOB_ERROR, "failed create window surface");
        return false;
    }

    if (!pick_phys_device()) {
        nob_log(NOB_ERROR, "failed find suitable GPU");
        return false;
    }

    if (!create_device()) {
        nob_log(NOB_ERROR, "failed to create logical device");
        return false;
    }

    return true;
}

bool cleanup()
{
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
    VkDebugUtilsMessengerCreateInfoEXT ci = {0};
    populated_debug_msgr_ci(&ci);
    instance_ci.pNext = &ci;
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

    for (size_t i = 0; i < NOB_ARRAY_LEN(validation_layers); i++) {
        bool layer_found = false;
        for (size_t j = 0; j < layer_count; j++) {
            if (strcmp(validation_layers[i], avail_layers[j].layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found)
            return false;
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
    VkDebugUtilsMessengerCreateInfoEXT ci = {0};
    populated_debug_msgr_ci(&ci);
    LOAD_PFN(vkCreateDebugUtilsMessengerEXT);
    if (vkCreateDebugUtilsMessengerEXT) {
        VkResult result = vkCreateDebugUtilsMessengerEXT(app.instance, &ci, NULL, &app.debug_msgr);
        return result == VK_SUCCESS;
    } else {
        return false;
    }
}

void populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *ci)
{
    ci->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci->pfnUserCallback = debug_callback;
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

    return indices.has_gfx;
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
#ifdef ENABLE_VALIDATION
    device_ci.enabledLayerCount = NOB_ARRAY_LEN(validation_layers);
    device_ci.ppEnabledLayerNames = validation_layers;
#endif

    if (VK_OK(vkCreateDevice(app.phys_device, &device_ci, NULL, &app.device))) {
        vkGetDeviceQueue(app.device, indices.gfx_idx, 0, &app.gfx_queue);
        vkGetDeviceQueue(app.device, indices.present_idx, 0, &app.present_queue);
        return true;
    } else {
        return false;
    }
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

int main()
{
    if (!init_window()) return 1;
    if (!init_vulkan()) return 1;
    if (!main_loop()) return 1;
    if (!cleanup()) return 1;

    return 0;
}