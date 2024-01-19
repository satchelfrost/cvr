#include "vk_cmd_man.h"
#include "vk_ctx.h"

bool cmd_buff_create(Vk_Cmd_Man *cmd_man);
bool cmd_syncs_create(Vk_Cmd_Man *cmd_man);
bool cmd_pool_create(Vk_Cmd_Man *cmd_man);

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
