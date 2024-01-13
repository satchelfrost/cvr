#ifndef VK_UTILS_H_
#define VK_UTILS_H_

#include <vulkan/vulkan.h>
#include "common.h"

typedef struct {
    uint32_t gfx_idx;
    bool has_gfx;
    uint32_t present_idx;
    bool has_present;
} QueueFamilyIndices;

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

#endif // VK_UTILS_H_
