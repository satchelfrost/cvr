#ifndef CVR_CMD_H_
#define CVR_CMD_H_

#include <vulkan/vulkan.h>
#include "common.h"

typedef struct {
    VkPhysicalDevice phys_device;
    VkDevice device;
    VkCommandPool pool;
    vec(VkCommandBuffer) buffs;
    vec(VkSemaphore) img_avail_sems;
    vec(VkSemaphore) render_finished_sems;
    vec(VkFence) fences;
} CVR_Cmd;

bool cmd_ctor(VkDevice device, VkPhysicalDevice phys_device, size_t num_cmd_objs);
void cmd_dtor();
bool cmd_buff_create(size_t num_cmd_buffs);
bool cmd_syncs_create(size_t num_syncs);
bool cmd_pool_create();

/* Quick one-off command buffer */
bool cmd_quick_begin(VkCommandBuffer *cmd_buff);
bool cmd_quick_end(VkQueue queue, VkCommandBuffer *cmd_buff);

#endif // CVR_CMD_H_