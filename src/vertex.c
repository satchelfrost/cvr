#include "vertex.h"

VkVertexInputBindingDescription get_binding_desc()
{
    VkVertexInputBindingDescription bindingDescription = {0};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

void get_attr_descs(VtxAttrDescs *attr_descs)
{
    VkVertexInputAttributeDescription desc = {0};
    desc.binding = 0;
    desc.location = 0;
    desc.format = VK_FORMAT_R32G32B32_SFLOAT;
    desc.offset = offsetof(Vertex, pos);
    nob_da_append(attr_descs, desc);

    desc.binding = 0;
    desc.location = 1;
    desc.format = VK_FORMAT_R32G32B32_SFLOAT;
    desc.offset = offsetof(Vertex, color);
    nob_da_append(attr_descs, desc);
}
