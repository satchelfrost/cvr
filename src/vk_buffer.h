#ifndef VK_BUFFER_H_
#define VK_BUFFER_H_

#include <stdbool.h>
#include <vulkan/vulkan.h>
#include "vk_cmd_man.h"

/* Manage vulkan buffer info */
typedef struct {
    VkDevice device;
    size_t size;
    size_t count;
    VkBuffer handle;
    VkDeviceMemory buff_mem;
    void *mapped;
} Vk_Buffer;

/* VK_Buffer must be set with device & size prior to calling this function */
bool vk_buff_init(Vk_Buffer *buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
void vk_buff_destroy(Vk_Buffer buffer);

/* Copies "size" bytes from src to dst buffer, a value of zero implies copying the whole src buffer */
bool vk_buff_copy(const Vk_Cmd_Man *cmd_man, VkQueue queue, Vk_Buffer src_buff, Vk_Buffer dst_buff, VkDeviceSize size);

#endif // !VK_BUFFER_H_
