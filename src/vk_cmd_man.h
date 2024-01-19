#ifndef VK_CMD_MAN_H_
#define VK_CMD_MAN_H_

#include "common.h"
#include <vulkan/vulkan.h>

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

/* Initializes a vulkan command manager, must set the physical device & logical device */
bool cmd_man_init(Vk_Cmd_Man *cmd_man);

/* Destroys a vulkan command manager */
void cmd_man_destroy(Vk_Cmd_Man *cmd_man);

/* Allocates and begins a temporary command buffer. Easy-to-use, not super efficent. */
bool cmd_quick_begin(const Vk_Cmd_Man *cmd_man, VkCommandBuffer *tmp_cmd_buff);

/* Ends and frees a temporary command buffer. Easy-to-use, not super efficent. */
bool cmd_quick_end(const Vk_Cmd_Man *cmd, VkQueue queue, VkCommandBuffer *tmp_cmd_buff);

#endif // CMD_MANAGER_H_
