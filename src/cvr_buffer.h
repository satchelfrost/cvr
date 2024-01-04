#ifndef CVR_BUFFER_H_
#define CVR_BUFFER_H_

#include <vulkan/vulkan.h>
#include "common.h"

typedef struct {
    VkDevice device;
    size_t size;
    size_t count;
    VkBuffer buff;
    VkDeviceMemory buff_mem;
    void *mapped;
} CVR_Buffer;

/* CVR_Buffer must be set with device & size prior to calling this constructor */
bool buffer_ctor(
    CVR_Buffer *buffer,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
);
void buffer_dtor(CVR_Buffer buffer);

/* Copies "size" bytes from src to dst buffer, a value of zero implies copying the whole src buffer */
bool copy_buff(VkQueue queue, CVR_Buffer src, CVR_Buffer dst, VkDeviceSize size);

#endif // CVR_BUFFER_H_