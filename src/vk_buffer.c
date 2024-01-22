#include "vk_buffer.h"
#include "vk_ctx.h"
#include "common.h"

bool vk_buff_init(Vk_Buffer *buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    bool result = true;
    VkBufferCreateInfo buffer_ci = {0};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cvr_chk(buffer->size, "Vk_Buffer must be set with size before calling constructor");
    buffer_ci.size = (VkDeviceSize) buffer->size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    cvr_chk(buffer->device, "Vk_Buffer must be set with device before calling constructor");
    vk_chk(vkCreateBuffer(buffer->device, &buffer_ci, NULL, &buffer->handle), "failed to create buffer");

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(buffer->device, buffer->handle, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(buffer->device, &alloc_ci, NULL, &buffer->buff_mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");

    vk_chk(vkBindBufferMemory(buffer->device, buffer->handle, buffer->buff_mem, 0), "failed to bind buffer memory");

defer:
    return result;
}

void vk_buff_destroy(Vk_Buffer buffer)
{
    if (!buffer.device) {
        nob_log(NOB_WARNING, "cannot destroy null buffer");
        return;
    }
    vkDestroyBuffer(buffer.device, buffer.handle, NULL);
    vkFreeMemory(buffer.device, buffer.buff_mem, NULL);
    buffer.handle = NULL;
}

bool vk_buff_copy(const Vk_Cmd_Man *cmd_man, VkQueue queue, Vk_Buffer src_buff, Vk_Buffer dst_buff, VkDeviceSize size)
{
    bool result = true;

    VkCommandBuffer tmp_cmd_buff;
    cvr_chk(cmd_quick_begin(cmd_man, &tmp_cmd_buff), "failed quick cmd begin");

    VkBufferCopy copy_region = {0};
    if (size) {
        copy_region.size = size;
        cvr_chk(size <= dst_buff.size, "Cannot copy buffer, size > dst buffer (won't fit)");
        cvr_chk(size <= src_buff.size, "Cannot copy buffer, size > src buffer (cannot copy more than what's available)");
    } else {
        cvr_chk(dst_buff.size >= src_buff.size, "Cannot copy buffer, dst buffer < src buffer (won't fit)");
        copy_region.size = src_buff.size;
    }

    vkCmdCopyBuffer(tmp_cmd_buff, src_buff.handle, dst_buff.handle, 1, &copy_region);

    cvr_chk(cmd_quick_end(cmd_man, queue, &tmp_cmd_buff), "failed quick cmd end");

defer:
    return result;
}

