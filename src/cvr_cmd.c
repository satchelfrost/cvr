#include "cvr_cmd.h"
#include "cvr_context.h"

CVR_Cmd cmd = {0};

bool cmd_ctor(VkDevice device, VkPhysicalDevice phys_device, size_t num_cmd_objs)
{
    bool result = true;
    cmd.device = device;
    cmd.phys_device = phys_device;
    cvr_chk(cmd_pool_create(), "failed to create command pool");
    cvr_chk(cmd_buff_create(num_cmd_objs), "failed to create cmd buffers");
    cvr_chk(cmd_syncs_create(num_cmd_objs), "failed to create cmd sync objects");

defer:
    return result;
}

void cmd_dtor()
{
    for (size_t i = 0; i < cmd.img_avail_sems.count; i++)
        vkDestroySemaphore(cmd.device, cmd.img_avail_sems.items[i], NULL);
    for (size_t i = 0; i < cmd.render_finished_sems.count; i++)
        vkDestroySemaphore(cmd.device, cmd.render_finished_sems.items[i], NULL);
    for (size_t i = 0; i < cmd.fences.count; i++)
        vkDestroyFence(cmd.device, cmd.fences.items[i], NULL);
    vkDestroyCommandPool(cmd.device, cmd.pool, NULL);

    nob_da_reset(cmd.buffs);
    nob_da_reset(cmd.img_avail_sems);
    nob_da_reset(cmd.render_finished_sems);
    nob_da_reset(cmd.fences);
}

bool cmd_pool_create()
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(cmd.phys_device);
    cvr_chk(indices.has_gfx, "failed to create command pool, no graphics queue");
    VkCommandPoolCreateInfo cmd_pool_ci = {0};
    cmd_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_ci.queueFamilyIndex = indices.gfx_idx;
    vk_chk(vkCreateCommandPool(cmd.device, &cmd_pool_ci, NULL, &cmd.pool), "failed to create command pool");

defer:
    return result;
}

bool cmd_buff_create(size_t num_cmd_buffs)
{
    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.commandPool = cmd.pool;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    nob_da_resize(&cmd.buffs, num_cmd_buffs);
    ci.commandBufferCount = cmd.buffs.count;
    return vk_ok(vkAllocateCommandBuffers(cmd.device, &ci, cmd.buffs.items));
}

bool cmd_syncs_create(size_t num_syncs)
{
    bool result = true;
    VkSemaphoreCreateInfo sem_ci = {0};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci = {0};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    nob_da_resize(&cmd.img_avail_sems, num_syncs);
    nob_da_resize(&cmd.render_finished_sems, num_syncs);
    nob_da_resize(&cmd.fences, num_syncs);
    VkResult vk_result;
    for (size_t i = 0; i < num_syncs; i++) {
        vk_result = vkCreateSemaphore(cmd.device, &sem_ci, NULL, &cmd.img_avail_sems.items[i]);
        vk_chk(vk_result, "failed to create semaphore");
        vk_result = vkCreateSemaphore(cmd.device, &sem_ci, NULL, &cmd.render_finished_sems.items[i]);
        vk_chk(vk_result, "failed to create semaphore");
        vk_result = vkCreateFence(cmd.device, &fence_ci, NULL, &cmd.fences.items[i]);
        vk_chk(vk_result, "failed to create fence");
    }

defer:
    return result;
}

bool cmd_quick_begin(VkCommandBuffer *cmd_buff)
{
    bool result = true;

    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ci.commandPool = cmd.pool;
    ci.commandBufferCount = 1;
    vk_chk(vkAllocateCommandBuffers(cmd.device, &ci, cmd_buff), "failed to create quick cmd");

    VkCommandBufferBeginInfo cmd_begin = {0};
    cmd_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_chk(vkBeginCommandBuffer(*cmd_buff, &cmd_begin), "failed to begin quick cmd");

defer:
    return result;
}

bool cmd_quick_end(VkQueue queue, VkCommandBuffer *cmd_buff)
{
    bool result = true;
    vk_chk(vkEndCommandBuffer(*cmd_buff), "failed to end cmd buffer");

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = cmd_buff;
    vk_chk(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "failed to submit quick cmd");
    vk_chk(vkQueueWaitIdle(queue), "failed to wait idle in quick cmd");

defer:
    vkFreeCommandBuffers(cmd.device, cmd.pool, 1, cmd_buff);
    return result;
}
