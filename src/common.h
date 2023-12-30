#ifndef COMMON_H_
#define COMMON_H_

#include "nob.h"
#include "nob_ext.h"

/* Common macro definitions */
#define LOAD_PFN(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(app.instance, #pfn)
#define UNUSED(x) (void)(x)
#define MIN_SEVERITY NOB_WARNING
#define VK_OK(x) ((x) == VK_SUCCESS)
#define CVR_CHK(expr, msg)           \
    do {                             \
        if (!(expr)) {               \
            nob_log(NOB_ERROR, msg); \
            nob_return_defer(false); \
        }                            \
    } while (0)
#define VK_CHK(vk_result, msg) CVR_CHK(VK_OK(vk_result), msg)
#define Vec(type) struct { \
    type *items;           \
    size_t capacity;       \
    size_t count;          \
}
#define CLAMP(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))

#endif // COMMON_H_