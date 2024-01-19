#include "ext_man.h"

/* Add various static extensions & validation layers here */
static const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
static const char *device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

Ext_Manager ext_manager = {0};

void init_ext_managner()
{
    if (ext_manager.inited) {
        nob_log(NOB_WARNING, "extension manager already initialized");
        return;
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(validation_layers); i++)
        nob_da_append(&ext_manager.validation_layers, validation_layers[i]);

    for (size_t i = 0; i < NOB_ARRAY_LEN(device_exts); i++)
        nob_da_append(&ext_manager.device_exts, device_exts[i]);

    ext_manager.inited = true;
}

void destroy_ext_manager()
{
    if (!ext_manager.inited) {
        nob_log(NOB_WARNING, "extension manager was not initialized");
        return;
    }

    nob_da_reset(ext_manager.validation_layers);
    nob_da_reset(ext_manager.device_exts);
    nob_da_reset(ext_manager.inst_exts);
}

bool inst_exts_satisfied()
{
    uint32_t avail_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, avail_exts);
    size_t unsatisfied_exts = ext_manager.inst_exts.count;
    for (size_t i = 0; i < ext_manager.inst_exts.count; i++) {
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(ext_manager.inst_exts.items[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
            }
        }
    }

    return false;
}

bool chk_validation_support()
{
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties avail_layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, avail_layers);
    size_t unsatisfied_layers = ext_manager.validation_layers.count;
    for (size_t i = 0; i < ext_manager.validation_layers.count; i++) {
        for (size_t j = 0; j < layer_count; j++) {
            if (strcmp(ext_manager.validation_layers.items[i], avail_layers[j].layerName) == 0) {
                if (--unsatisfied_layers == 0)
                    return true;
            }
        }
    }

    return true;
}

bool device_exts_supported(VkPhysicalDevice phys_device)
{
    uint32_t avail_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &avail_ext_count, avail_exts);
    uint32_t unsatisfied_exts = ext_manager.device_exts.count; 
    for (size_t i = 0; i < ext_manager.device_exts.count; i++) {
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(ext_manager.device_exts.items[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
            }
        }
    }

    return false;
}
