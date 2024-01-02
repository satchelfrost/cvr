#include "cvr_buffer.h"
#include "app_utils.h"
#include "cvr_cmd.h"

bool buffer_ctor(
    CVR_Buffer *buffer,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties)
{
    bool result = true;
    VkBufferCreateInfo buffer_ci = {0};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cvr_chk(buffer->size, "CVR_Buffer must be set with size before calling constructor");
    buffer_ci.size = (VkDeviceSize) buffer->size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    cvr_chk(buffer->device, "CVR_Buffer must be set with device before calling constructor");
    vk_chk(vkCreateBuffer(buffer->device, &buffer_ci, NULL, &buffer->buff), "failed to create buffer");

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(buffer->device, buffer->buff, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(buffer->device, &alloc_ci, NULL, &buffer->buff_mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");

    vk_chk(vkBindBufferMemory(buffer->device, buffer->buff, buffer->buff_mem, 0), "failed to bind buffer memory");

defer:
    return result;
}

bool copy_buff(VkQueue queue, CVR_Buffer src, CVR_Buffer dst, VkDeviceSize size)
{
    bool result = true;

    VkCommandBuffer cmd_buff;
    cvr_chk(cmd_quick_begin(&cmd_buff), "failed quick cmd begin");

    VkBufferCopy copy_region = {0};
    if (size) {
        copy_region.size = size;
        cvr_chk(size <= dst.size, "Cannot copy buffer, size > dst buffer");
        cvr_chk(size <= src.size, "Cannot copy buffer, size > src buffer");
    } else {
        cvr_chk(dst.size >= src.size, "Cannot copy buffer, dst buffer < src buffer");
        copy_region.size = src.size;
    }

    vkCmdCopyBuffer(cmd_buff, src.buff, dst.buff, 1, &copy_region);

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