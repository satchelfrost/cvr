#include "cvr_buffer.h"
#include "app_utils.h"
#include "cvr_cmd.h"

bool buffer_ctor(
    CVR_Buffer *buffer,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties)
{
    bool result = true;
    buffer->device = device;
    VkBufferCreateInfo buffer_ci = {0};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_chk(vkCreateBuffer(device, &buffer_ci, NULL, &buffer->buff), "failed to create buffer");

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(device, buffer->buff, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(device, &alloc_ci, NULL, &buffer->buff_mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");

    vk_chk(vkBindBufferMemory(device, buffer->buff, buffer->buff_mem, 0), "failed to bind buffer memory");

defer:
    return result;
}

bool copy_buff(VkQueue queue, VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    bool result = true;

    VkCommandBuffer cmd_buff;
    cvr_chk(cmd_quick_begin(&cmd_buff), "failed quick cmd begin");

    VkBufferCopy copy_region = {0};
    copy_region.size = size;
    vkCmdCopyBuffer(cmd_buff, src, dst, 1, &copy_region);

    cvr_chk(cmd_quick_end(queue, &cmd_buff), "failed quick cmd end");

defer:
    return result;
}

void buffer_dtor(CVR_Buffer buffer)
{
    if (!buffer.device) {
        nob_log(NOB_WARNING, "cannot destroy null buffer");
        return;
    }
    vkDestroyBuffer(buffer.device, buffer.buff, NULL);
    vkFreeMemory(buffer.device, buffer.buff_mem, NULL);
}