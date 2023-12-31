#ifndef VERTEX_H_
#define VERTEX_H_

#include "common.h"
#include "ext/raylib-5.0/raymath.h"
#include <vulkan/vulkan.h>

typedef struct {
    Vector2 pos;
    Vector3 color;
} Vertex; 

VkVertexInputBindingDescription get_binding_desc();
typedef vec(VkVertexInputAttributeDescription) VtxAttrDescs;
void get_attr_descs(VtxAttrDescs *attr_descs);

#endif