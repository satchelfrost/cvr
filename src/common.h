#ifndef COMMON_H_
#define COMMON_H_

#include "nob.h"
#include "nob_ext.h"

/* Common macro definitions */
#define load_pfn(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(app.instance, #pfn)
#define unused(x) (void)(x)
#define MIN_SEVERITY NOB_WARNING
#define vk_ok(x) ((x) == VK_SUCCESS)
#define cvr_chk(expr, msg)           \
    do {                             \
        if (!(expr)) {               \
            nob_log(NOB_ERROR, msg); \
            nob_return_defer(false); \
        }                            \
    } while (0)
#define vk_chk(vk_result, msg) cvr_chk(vk_ok(vk_result), msg)
#define vec(type) struct { \
    type *items;           \
    size_t capacity;       \
    size_t count;          \
}
#define clamp(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))

#endif // COMMON_H_