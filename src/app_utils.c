#include "app_utils.h"
#include "common.h"
#include "app.h"
#include "ext_man.h"

extern App app; // app.c

bool is_device_suitable(VkPhysicalDevice phys_device)
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(phys_device);
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
    nob_log(log_lvl, p_callback_data->pMessage);
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
        return vk_ok(vkCreateDebugUtilsMessengerEXT(app.instance, &debug_msgr_ci, NULL, &app.debug_msgr));
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

        extent.width = clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

void cleanup_swpchain()
{
    for (size_t i = 0; i < app.frame_buffs.count; i++)
        vkDestroyFramebuffer(app.device, app.frame_buffs.items[i], NULL);
    for (size_t i = 0; i < app.swpchain_img_views.count; i++)
        vkDestroyImageView(app.device, app.swpchain_img_views.items[i], NULL);
    vkDestroySwapchainKHR(app.device, app.swpchain, NULL);
}

bool find_mem_type_idx(uint32_t type, VkMemoryPropertyFlags properties, uint32_t *idx)
{
    VkPhysicalDeviceMemoryProperties mem_properites = {0};
    vkGetPhysicalDeviceMemoryProperties(app.phys_device, &mem_properites);
    for (uint32_t i = 0; i < mem_properites.memoryTypeCount; i++) {
        if (type & (1 << i) && (mem_properites.memoryTypes[i].propertyFlags & properties) == properties) {
            *idx = i;
            return true;
        }
    }

    return false;
}