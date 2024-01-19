#ifndef EXT_MAN_H_
#define EXT_MAN_H_

#include "common.h"
#include <vulkan/vulkan.h>

/* Manage vulkan extensions*/
typedef vec(const char*) Extensions;
typedef struct {
    Extensions validation_layers;
    Extensions device_exts;
    Extensions inst_exts;
    int inited;
} Ext_Manager;

void init_ext_managner();
void destroy_ext_manager();
bool inst_exts_satisfied();
bool chk_validation_support();
bool device_exts_supported(VkPhysicalDevice phys_device);

#endif // EXT_MAN_H_
