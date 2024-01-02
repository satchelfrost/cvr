#ifndef CVR_BUFFER_H_
#define CVR_BUFFER_H_

#include <vulkan/vulkan.h>
#include "common.h"

typedef struct {
    VkDevice device;
    VkBuffer buff;
    VkDeviceMemory buff_mem;
} CVR_Buffer;

bool buffer_ctor(
    CVR_Buffer *buffer,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
);
void buffer_dtor(CVR_Buffer buffer);
bool copy_buff(VkQueue queue, VkBuffer src, VkBuffer dst, VkDeviceSize size);

#endif // CVR_BUFFER_H_