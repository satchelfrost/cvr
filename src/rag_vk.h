#ifndef RAG_VK_H_
#define RAG_VK_H_

#define RVK_ASSERT assert
#define RVK_REALLOC realloc
#define RVK_FREE free

#ifdef PLATFORM_DESKTOP_GLFW
#define RVK_EXIT_APP RVK_ASSERT(0)
#else
#define RVK_EXIT_APP
#endif

// easy: requires little thinking just need to go through the errors
// ----------------------------------------------------------------------------------------------
// TODO: destroy functions should not be passing things by pointer
// TODO: get rid of most of RVK_SUCCEEDED 
// TODO: get rid of bools for things like buff init (unless I find it useful)
// TODO: upload version for vtx and idx buff
// ----------------------------------------------------------------------------------------------
// medium: requires some thinking but there's common approaches to solve
// TODO: consider using VK_WHOLE_SIZE as an option for buffer copy
// TODO: feature requesting should be little more robust, required to fix examples that require features
// TODO: device specification should be more robust
// ----------------------------------------------------------------------------------------------
// hard: not clear what the best solution is, meaning I will have to make thoughtful compromises
// ----------------------------------------------------------------------------------------------
// TODO: texture and image needs to be rethought for example memory should be in texture
// TODO: rework rvk_img_init in the same way I re-wored rvk_buff_init

#include <stdint.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef PLATFORM_DESKTOP_GLFW
#include <GLFW/glfw3.h>
#endif

#ifdef PLATFORM_ANDROID_QUEST
#include <android_native_app_glue.h>
#include <android/log.h>
#endif // PLATFORM_ANDROID_QUEST

#ifndef APP_NAME
    #define APP_NAME "app"
#endif
#ifndef MIN_SEVERITY
    #define MIN_SEVERITY RVK_WARNING
#endif

#define VK_FLAGS_NONE 0
#define RVK_LOAD_PFN(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(rvk_ctx.instance, #pfn)
#define RVK_SUCCEEDED(x) ((x) == VK_SUCCESS)
#define clamp(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))
#define RVK_ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))

/* note: these are a *nearly* one-to-one mapping of the vulkan equivalent (i.e. VK_DESCRIPTOR_TYPE_*),
 * but VK_DESCRIPTOR_TYPE_MAX_ENUM = 0x7FFFFFFF, is too huge for a static array, so
 * I use DESCRIPTOR_TYPE_COUNT instead */
typedef enum {
    RVK_DESCRIPTOR_TYPE_SAMPLER,
    RVK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    RVK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    RVK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    RVK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
    RVK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
    RVK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    RVK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    RVK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    RVK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    RVK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
    RVK_DESCRIPTOR_TYPE_COUNT,
} Rvk_Descriptor_Type;

typedef struct {
    VkDescriptorSetLayout handle;
    uint32_t desc_count[RVK_DESCRIPTOR_TYPE_COUNT];
} Rvk_Descriptor_Set_Layout;

#define MAX_DESCRIPTOR_SETS 100
typedef struct {
    VkDescriptorPool pool;
    /* Please note that pool usage tracking is only good
     * for judging whether the pool capacity is close to full.
     * it is only incremented when you call: rvk_descriptor_pool_arena_alloc_set(...)
     * it is only decremented when you call: rvk_descriptor_pool_arena_reset(...)
     * */
    uint32_t pools_usage[RVK_DESCRIPTOR_TYPE_COUNT];
} Rvk_Descriptor_Pool_Arena;

#define RVK_MAX_SWAPCHAIN_IMAGES 5
typedef struct {
    VkSwapchainKHR handle;
    VkImage imgs[RVK_MAX_SWAPCHAIN_IMAGES];
    VkImageView img_views[RVK_MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer frame_buffs[RVK_MAX_SWAPCHAIN_IMAGES];
    uint32_t img_count;
    bool buff_resized;
} Rvk_Swapchain;

typedef enum {
    RVK_BUFFER_TYPE_ANY,
    RVK_BUFFER_TYPE_VERTEX,
    RVK_BUFFER_TYPE_INDEX,
    RVK_BUFFER_TYPE_COMPUTE,
    RVK_BUFFER_TYPE_UNIFORM,
    RVK_BUFFER_TYPE_STAGING,
    RVK_BUFFER_TYPE_COUNT,
} Rvk_Buffer_Type;

typedef struct {
    size_t size;
    size_t count;
    VkBuffer handle;
    VkDeviceMemory mem;
    void *mapped;
    void *data;
    VkDescriptorBufferInfo info;
    Rvk_Buffer_Type type;
} Rvk_Buffer;

typedef struct {
    VkExtent2D extent;
    VkImage handle;
    VkDeviceMemory mem;
    VkImageAspectFlags aspect_mask;
    VkFormat format;
} Rvk_Image;

typedef struct {
    VkImageView view;
    VkSampler sampler;
    Rvk_Image img;
    VkDescriptorImageInfo info;
} Rvk_Texture;

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_msgr;
    VkDebugReportCallbackEXT report_callback;
    VkPhysicalDevice phys_device;
    VkDevice device;
    uint32_t queue_idx;
    VkQueue unified_queue;
    VkCommandPool pool;
    VkCommandBuffer cmd_buff;
    VkSemaphore img_avail_sem;
    VkSemaphore render_fin_sem;
    VkFence fence;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_fmt;
    VkExtent2D extent;
    VkRenderPass render_pass;
    Rvk_Swapchain swapchain;
    Rvk_Image depth_img;
    VkImageView depth_img_view;
    bool using_validation;
} Rvk_Context;

typedef struct {
    int width;
    int height;
    const char *title;
} Rvk_Config;
#define rvk_init(...) rvk_init_((Rvk_Config){__VA_ARGS__})
void rvk_init_(Rvk_Config cfg);

void rvk_destroy(void);
void rvk_instance_init(void);
void rvk_device_init(void);
void rvk_wait_idle(void);
void rvk_swapchain_init(void);
void rvk_img_views_init(void);
void rvk_img_view_init(Rvk_Image img, VkImageView *img_view);
void rvk_sst_pl_init(VkPipelineLayout pl_layout, VkPipeline *pl);
void rvk_compute_pl_init(const char *shader_name, VkPipelineLayout pl_layout, VkPipeline *pipeline);
void rvk_shader_mod_init(const char *file_name, VkShaderModule *module);
void rvk_render_pass_init(void);
void rvk_create_render_pass(VkRenderPassCreateInfo *rp_create_info, VkRenderPass *render_pass);
VkFormat rvk_surface_fmt(void);
void rvk_destroy_render_pass(VkRenderPass render_pass);
void rvk_frame_buffs_init(void);
bool rvk_create_frame_buff(uint32_t w, uint32_t h, VkImageView *atts, uint32_t att_count, VkRenderPass rp, VkFramebuffer *fb);
void rvk_destroy_frame_buff(VkFramebuffer frame_buff);
void rvk_recreate_swapchain(void);
void rvk_depth_init(void);
void rvk_destroy_pl_res(VkPipeline pipeline, VkPipelineLayout pl_layout);
VkCommandBuffer rvk_get_cmd_buff(void);
VkCommandBuffer rvk_get_comp_buff(void);
void rvk_reset_pool(VkDescriptorPool pool);
bool rvk_update(void);
double rvk_dt(void);

/* platform specifics */
#ifdef PLATFORM_DESKTOP_GLFW
bool rvk_glfw_surface_init();
void rvk_glfw_wait_resize_frame_buffer();
bool rvk_glfw_init(int width, int height, const char *title);
void rvk_glfw_destroy();
bool rvk_window_should_close();
void rvk_poll_glfw_events();
double rvk_get_glfw_time();
#endif

#ifdef PLATFORM_ANDROID_QUEST
void rvk_set_android_asset_man(AAssetManager *aam);
#endif

void rvk_descriptor_pool_arena_init(Rvk_Descriptor_Pool_Arena *arena);
void rvk_descriptor_pool_arena_reset(Rvk_Descriptor_Pool_Arena *arena);
bool rvk_descriptor_pool_arena_alloc_set(Rvk_Descriptor_Pool_Arena *arena, Rvk_Descriptor_Set_Layout *ds_layout, VkDescriptorSet *set);
const char *rvk_desc_type_to_str(Rvk_Descriptor_Type type);
void rvk_log_descriptor_pool_usage(Rvk_Descriptor_Pool_Arena arena);
void rvk_log_descriptor_layout_usage(Rvk_Descriptor_Set_Layout layout, const char *shader_name);
void rvk_descriptor_pool_arena_destroy(Rvk_Descriptor_Pool_Arena arena);

typedef struct {
    VkPipelineLayout pl_layout;
    const char *vert;
    const char *frag;
    VkPrimitiveTopology topology;
    VkPolygonMode polygon_mode;
    VkVertexInputAttributeDescription *vert_attrs;
    size_t vert_attr_count;
    VkVertexInputBindingDescription *vert_bindings;
    size_t vert_binding_count;
    VkRenderPass render_pass;
} Pipeline_Config;

bool rvk_pl_layout_init(VkPipelineLayoutCreateInfo ci, VkPipelineLayout *pl_layout);
void rvk_basic_pl_init(Pipeline_Config config, VkPipeline *pl);

void rvk_wait_to_begin_gfx();
void rvk_begin_rec_gfx();
void rvk_wait_reset();
void rvk_wait_for_fences(VkFence *fences, uint32_t fence_count);
void rvk_reset_fences(VkFence *fences, uint32_t fence_count);
void rvk_reset_command_buffer(VkCommandBuffer cmd_buff);
void rvk_begin_command_buffer(VkCommandBuffer cmd_buff);
void rvk_end_rec_gfx();
void rvk_end_command_buffer(VkCommandBuffer cmd_buff);
void rvk_begin_render_pass(float r, float g, float b, float a);
void rvk_begin_offscreen_render_pass(float r, float g, float b, float a, VkRenderPass rp, VkFramebuffer fb, VkExtent2D extent);
void rvk_end_render_pass();
void rvk_cmd_end_render_pass(VkCommandBuffer cmd_buff);
void rvk_submit_gfx();

typedef struct {
    const void* p_next;
    uint32_t wait_semaphore_count;
    const VkSemaphore* p_wait_semaphores;
    const VkPipelineStageFlags* p_wait_dst_stage_mask;
    uint32_t command_buffer_count;
    const VkCommandBuffer* p_command_buffers;
    uint32_t signal_semaphore_count;
    const VkSemaphore* p_signal_semaphores;
} Rvk_Submit_Info;
#define rvk_queue_submit(queue, fence, ...) rvk_queue_submit_(queue, fence, (Rvk_Submit_Info){__VA_ARGS__})
void rvk_queue_submit_(VkQueue queue, VkFence fence, Rvk_Submit_Info rvk_si);

typedef struct {
    const void* p_next;
    VkRenderPass render_pass;
    VkFramebuffer framebuffer;
    VkRect2D render_area;
    uint32_t clear_value_count;
    const VkClearValue* p_clear_values;
} Rvk_Render_Pass_Begin_Info;
#define rvk_cmd_begin_render_pass(cmd_buff, ...) rvk_cmd_begin_render_pass_(cmd_buff, (Rvk_Render_Pass_Begin_Info){__VA_ARGS__});
bool rvk_cmd_begin_render_pass_(VkCommandBuffer cmd_buff, Rvk_Render_Pass_Begin_Info rvk_bi);

void rvk_draw(VkPipeline pl, VkPipelineLayout pl_layout, Rvk_Buffer vtx_buff, Rvk_Buffer idx_buff, void *float16_mvp);
void rvk_bind_gfx(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet *ds, size_t ds_count);
void rvk_bind_gfx_extent(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet *ds, size_t ds_count, VkExtent2D extent);
void rvk_draw_buffers(Rvk_Buffer vtx_buff, Rvk_Buffer idx_buff);
void rvk_draw_points(Rvk_Buffer vtx_buff, void *float16_mvp, VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet *ds_sets, size_t ds_set_count);
void rvk_draw_sst(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds);

bool rvk_rec_compute();
bool rvk_submit_compute();
bool rvk_compute_fence_wait();
bool rvk_end_rec_compute();
void rvk_compute(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds, size_t x, size_t y, size_t z);
void rvk_dispatch(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds, size_t x, size_t y, size_t z);
void rvk_push_const(VkPipelineLayout pl_layout, VkShaderStageFlags flags, uint32_t size, void *value);
void rvk_compute_pl_barrier();

bool rvk_buff_init(size_t size, size_t count, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props, Rvk_Buffer_Type type, void *data, Rvk_Buffer *buffer);
bool rvk_uniform_buff_init(size_t size, void *data, Rvk_Buffer *buffer);
bool rvk_comp_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer);
bool rvk_vtx_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer);
bool rvk_idx_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer);
void rvk_stage_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer);
void rvk_buff_destroy(Rvk_Buffer buffer);
bool rvk_buff_map(Rvk_Buffer *buff);
bool rvk_buff_unmap(Rvk_Buffer *buff);
void rvk_buff_staged_upload(Rvk_Buffer buff);
const char *rvk_buff_type_as_str(Rvk_Buffer_Type type);

/* Copies "size" bytes from src to dst buffer, a value of zero implies copying the whole src buffer */
void rvk_buff_copy(Rvk_Buffer dst_buff, Rvk_Buffer src_buff, VkDeviceSize size);

void rvk_storage_tex_init(Rvk_Texture *texture, VkExtent2D extent);
void rvk_pl_barrier(VkImageMemoryBarrier barrier);

/* custom barrier to ensure compute shaders are done before sampling image (useful for software rasterization) */
void rvk_raster_sampler_barrier(VkImage img);
void rvk_depth_img_barrier(VkImage depth_img);
void rvk_color_img_barrier(VkImage color_img);
void rvk_swapchain_img_barrier(void);
uint32_t rvk_advance_frame(void);
uint32_t rvk_get_frame_idx(void);

void rvk_ds_layout_init(VkDescriptorSetLayoutBinding *bindings, size_t b_count, Rvk_Descriptor_Set_Layout *layout);
// multiplier: the number of times you've used a descriptor set layout to allocate descriptor sets
// its only for tracking purposes, but tracking descriptor sets is useful
void rvk_destroy_rvk_descriptor_set_layout(Rvk_Descriptor_Pool_Arena arena, Rvk_Descriptor_Set_Layout layout, uint32_t multiplier);

void rvk_create_ds_pool(VkDescriptorPoolCreateInfo pool_ci, VkDescriptorPool *pool);
void rvk_destroy_ds_pool(VkDescriptorPool pool);
void rvk_destroy_descriptor_set_layout(VkDescriptorSetLayout layout);
bool rvk_alloc_ds(VkDescriptorSetAllocateInfo alloc, VkDescriptorSet *sets);
void rvk_update_ds(size_t count, VkWriteDescriptorSet *writes);


typedef enum {
    RVK_INFO,
    RVK_WARNING,
    RVK_ERROR,
} Rvk_Log_Level;

void rvk_log(Rvk_Log_Level level, const char *fmt, ...);

void rvk_populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *debug_msgr_ci);
Rvk_Log_Level translate_msg_severity(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity);
void rvk_setup_debug_msgr();
bool rvk_setup_report_callback();
bool rvk_set_queue_fam_indices(VkPhysicalDevice phys_device);
bool rvk_set_gfx_capable_queue_idx(VkPhysicalDevice phys_device);
bool rvk_swapchain_adequate(VkPhysicalDevice phys_device);
void rvk_choose_swapchain_fmt();
VkPresentModeKHR rvk_choose_present_mode();
VkExtent2D rvk_choose_swp_extent();
bool rvk_is_device_suitable(VkPhysicalDevice phys_device);
void rvk_pick_phys_device();
void rvk_destroy_swapchain();
bool rvk_find_mem_type_idx(uint32_t type, VkMemoryPropertyFlags properties, uint32_t *idx);
uint32_t rvk_get_unified_gfx_and_present_queue_idx(VkPhysicalDevice phys_device);
bool rvk_has_unified_gfx_and_present_queue(VkPhysicalDevice phys_device);
void rvk_img_init(Rvk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
void rvk_img_copy(VkImage dst_img, VkBuffer src_buff, VkExtent2D extent);
Rvk_Texture rvk_load_texture(void *data, size_t width, size_t height, VkFormat fmt);
void rvk_unload_texture(Rvk_Texture texture);
bool rvk_transition_img_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
bool rvk_sampler_init(VkSampler *sampler);
int rvk_format_to_size(VkFormat fmt);

#define rvk_return_defer(value) do { result = (value); goto defer; } while(0)

// Initial capacity of a dynamic array
#define VK_DA_INIT_CAP 256

#define rvk_da_append(da, item)                                                          \
    do {                                                                                \
        if ((da)->count >= (da)->capacity) {                                            \
            (da)->capacity = (da)->capacity == 0 ? VK_DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = RVK_REALLOC((da)->items, (da)->capacity*sizeof(*(da)->items)); \
            RVK_ASSERT((da)->items != NULL && "\"Buy more RAM lol\"\n\t\t-Tsoding");     \
        }                                                                               \
                                                                                        \
        (da)->items[(da)->count++] = (item);                                            \
    } while (0)

#define rvk_da_resize(da, new_size)                                                    \
    do {                                                                              \
        (da)->capacity = (da)->count = new_size;                                      \
        (da)->items = RVK_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items)); \
    } while (0)

#define rvk_da_append_many(da, new_items, new_items_count)                                   \
    do {                                                                                    \
        if ((da)->count + new_items_count > (da)->capacity) {                               \
            if ((da)->capacity == 0) {                                                      \
                (da)->capacity = VK_DA_INIT_CAP;                                            \
            }                                                                               \
            while ((da)->count + new_items_count > (da)->capacity) {                        \
                (da)->capacity *= 2;                                                        \
            }                                                                               \
            (da)->items = RVK_REALLOC((da)->items, (da)->capacity*sizeof(*(da)->items));     \
            RVK_ASSERT((da)->items != NULL && "\"Buy more RAM lol\"\n\t\t-Tsoding");         \
        }                                                                                   \
        memcpy((da)->items + (da)->count, new_items, new_items_count*sizeof(*(da)->items)); \
        (da)->count += new_items_count;                                                     \
    } while (0)

#define rvk_da_free(da) RVK_FREE((da).items)

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} Rvk_String_Builder;

bool rvk_read_entire_file(const char *path, Rvk_String_Builder *sb);

#define rvk_sb_free(sb) RVK_FREE((sb).items)

// Append a sized buffer to a string builder
#define rvk_sb_append_buf(sb, buf, size) rvk_da_append_many(sb, buf, size)


typedef struct {
    const void* p_next;
    VkCommandPool command_pool;
    VkCommandBufferLevel level;
    uint32_t command_buffer_count;
} Rvk_Command_Buffer_Allocate_Info;
#define rvk_allocate_command_buffer(buff, ...) rvk_allocate_command_buffer_(buff, (Rvk_Command_Buffer_Allocate_Info){__VA_ARGS__})
bool rvk_allocate_command_buffer_(VkCommandBuffer *buff, Rvk_Command_Buffer_Allocate_Info ci);

void rvk_cmd_syncs_init();
void rvk_cmd_pool_init();
void rvk_create_semaphore(VkSemaphore *semaphore);
void rvk_create_fence(VkFence *fence);
void rvk_destroy_semaphore(VkSemaphore semaphore);
void rvk_destroy_fence(VkFence fence);

/* Allocates and begins a temporary command buffer. Easy-to-use, not super efficent. */
VkCommandBuffer rvk_cmd_quick_begin(void);

/* Ends and frees a temporary command buffer. Easy-to-use, not super efficent. */
void rvk_cmd_quick_end(VkCommandBuffer *tmp_cmd_buff);

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Rvk_Instance_Exts;
bool rvk_inst_exts_satisfied();
bool rvk_validation_supported();
bool rvk_device_exts_supported(VkPhysicalDevice phys_device);
const char *vk_res_to_str(VkResult result);

void rvk_handle_bad_vk_result(VkResult result, const char* function);
#define RAG_VK(func) rvk_handle_bad_vk_result(func, #func);


#endif // RAG_VK_H_

/***********************************************************************************
*
*   Vulkan Context Implementation
*
************************************************************************************/

#ifdef RAG_VK_IMPLEMENTATION

#include <vulkan/vulkan.h>

#define Z_NEAR 0.01
#define Z_FAR 500.0

Rvk_Context rvk_ctx = {0};

/* swapchain image index */
static uint32_t rvk_img_idx = 0;

/* various extensions & validation layers here */
static const char *rvk_validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
static const char *rvk_device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
Rvk_Instance_Exts rvk_inst_exts = {0};

#ifdef PLATFORM_ANDROID_QUEST
AAssetManager *rvk_aam = NULL;
#endif

void rvk_handle_bad_vk_result(VkResult result, const char* function)
{
    if (!RVK_SUCCEEDED(result)) {
        rvk_log(RVK_ERROR, "Vulkan Error: %s : %s", function, vk_res_to_str(result));
        RVK_EXIT_APP; // Can be defined to exit how you please
    }
}

void rvk_log(Rvk_Log_Level level, const char *fmt, ...)
{
#if defined(PLATFORM_ANDROID_QUEST)
    va_list args;
    va_start(args, fmt);
    switch (level) {
    case RVK_INFO:
         __android_log_vprint(ANDROID_LOG_INFO,  APP_NAME, fmt, args);
        break;
    case RVK_WARNING:
         __android_log_vprint(ANDROID_LOG_WARN,  APP_NAME, fmt, args);
        break;
    case RVK_ERROR:
         __android_log_vprint(ANDROID_LOG_ERROR,  APP_NAME, fmt, args);
        break;
    }
#elif defined(PLATFORM_DESKTOP_GLFW)
    switch (level) {
    case RVK_INFO:
        fprintf(stderr, "[RVK][INFO] ");
        break;
    case RVK_WARNING:
        fprintf(stderr, "[RVK][WARNING] ");
        break;
    case RVK_ERROR:
        fprintf(stderr, "[RVK][ERROR] ");
        break;
    default:
        RVK_EXIT_APP;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
#else
    RVK_EXIT_APP;
#endif // end of platform defines
}

const char *vk_res_to_str(VkResult res)
{
    switch (res) {
    case VK_SUCCESS:                                           return "VK_SUCCESS";
    case VK_NOT_READY:                                         return "VK_NOT_READY";
    case VK_TIMEOUT:                                           return "VK_TIMEOUT";
    case VK_EVENT_SET:                                         return "VK_EVENT_SET";
    case VK_EVENT_RESET:                                       return "VK_EVENT_RESET";
    case VK_INCOMPLETE:                                        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:                          return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:                        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:                       return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:                                 return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:                           return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:                           return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:                       return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:                         return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:                         return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:                            return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:                        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:                             return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:                                     return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:                          return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:                     return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:                               return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:              return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED:                         return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_SURFACE_LOST_KHR:                            return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:                    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:                                    return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:                             return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:                    return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:                       return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:                           return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:               return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:      return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:   return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:      return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:       return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:         return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_KHR:                           return "VK_ERROR_NOT_PERMITTED_KHR";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:         return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR:                                   return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:                                   return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:                            return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:                        return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:            return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:                   return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
    case VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT:              return "VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT";
    default: return "unrecognized vkresult";
    }
}

bool rvk_read_entire_file(const char *path, Rvk_String_Builder *sb)
{
    bool result = true;
    size_t buf_size = 32*1024;
    char *buf = RVK_REALLOC(NULL, buf_size);
    RVK_ASSERT(buf != NULL && "\"Buy more RAM lool!!\"\n\t\t-Tsoding");
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        rvk_log(RVK_ERROR, "Could not open %s for reading: %s", path, strerror(errno));
        rvk_return_defer(false);
    }

    size_t n = fread(buf, 1, buf_size, f);
    while (n > 0) {
        rvk_sb_append_buf(sb, buf, n);
        n = fread(buf, 1, buf_size, f);
    }
    if (ferror(f)) {
        rvk_log(RVK_ERROR, "Could not read %s: %s\n", path, strerror(errno));
        rvk_return_defer(false);
    }

defer:
    RVK_FREE(buf);
    if (f) fclose(f);
    return result;
}

/***********************************************************************************
*
*   If using GLFW on desktop: #define PLATFORM_DESKTOP_GLFW
*
************************************************************************************/

#ifdef PLATFORM_DESKTOP_GLFW

GLFWwindow *rvk_glfw_window;

bool rvk_glfw_surface_init()
{
    if (RVK_SUCCEEDED(glfwCreateWindowSurface(rvk_ctx.instance, rvk_glfw_window, NULL, &rvk_ctx.surface))) {
        return true;
    } else {
        rvk_log(RVK_ERROR, "failed to initialize glfw window surface");
        return false;
    }
}

void rvk_glfw_wait_resize_frame_buffer()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(rvk_glfw_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(rvk_glfw_window, &width, &height);
        glfwWaitEvents();
    }
}

bool rvk_glfw_init(int width, int height, const char *title)
{
    if (rvk_glfw_window) {
        rvk_log(RVK_ERROR, "window handle was already initialized");
        return false;
    }
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    rvk_glfw_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!rvk_glfw_window) return false;

    return true;
}

void rvk_glfw_destroy()
{
    glfwDestroyWindow(rvk_glfw_window);
    glfwTerminate();
}

bool rvk_window_should_close()
{
    return glfwWindowShouldClose(rvk_glfw_window);
}

void rvk_poll_glfw_events()
{
    glfwPollEvents();
}

double rvk_get_glfw_time()
{
    return glfwGetTime();
}

#endif // PLATFORM_DESKTOP_GLFW

/***********************************************************************************
*
*   If using GLFW on desktop: #define PLATFORM_ANDROID_QUEST
*
************************************************************************************/

#ifdef PLATFORM_ANDROID_QUEST
void rvk_set_android_asset_man(AAssetManager *aam)
{
    rvk_aam = aam;
}
#endif // PLATFORM_ANDROID_QUEST

#define RVK_DEFAULT_WINDOW_DIM 500

void rvk_init_(Rvk_Config cfg)
{

#ifdef PLATFORM_DESKTOP_GLFW
    (void) cfg;
    if (!rvk_glfw_window) {
        rvk_log(RVK_ERROR, "must explicitly initialize glfw window manually or use rvk_glfw_init");
        RVK_EXIT_APP;
    }
#else
    rvk_log(RVK_ERROR, "currently rvk_init only supports PLATFORM_DESKTOP_GLFW");
    RVK_EXIT_APP;
#endif

    rvk_instance_init();
#ifdef VK_VALIDATION
    rvk_setup_debug_msgr();
#endif
#ifdef PLATFORM_DESKTOP_GLFW
    rvk_glfw_surface_init();
#else
    rvk_log(RVK_ERROR, "currently rvk_init only supports PLATFORM_DESKTOP_GLFW");
    RVK_EXIT_APP;
#endif

    /* picking physical device also sets queue family indices in ctx */
    rvk_pick_phys_device();

    rvk_device_init();
    rvk_swapchain_init();
    rvk_img_views_init();
    rvk_render_pass_init();
    rvk_depth_init();
    rvk_frame_buffs_init();
    rvk_cmd_pool_init();
    rvk_allocate_command_buffer(&rvk_ctx.cmd_buff);
    rvk_cmd_syncs_init();
}

void rvk_destroy()
{
    vkDeviceWaitIdle(rvk_ctx.device);

    vkDestroySemaphore(rvk_ctx.device, rvk_ctx.img_avail_sem, NULL);
    vkDestroySemaphore(rvk_ctx.device, rvk_ctx.render_fin_sem, NULL);
    vkDestroyFence(rvk_ctx.device, rvk_ctx.fence, NULL);
    vkDestroyCommandPool(rvk_ctx.device, rvk_ctx.pool, NULL);

    rvk_destroy_swapchain();
    vkDeviceWaitIdle(rvk_ctx.device);
    vkDestroyRenderPass(rvk_ctx.device, rvk_ctx.render_pass, NULL);
    vkDestroyDevice(rvk_ctx.device, NULL);
#ifdef VK_VALIDATION
    RVK_LOAD_PFN(vkDestroyDebugUtilsMessengerEXT);
    if (vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(rvk_ctx.instance, rvk_ctx.debug_msgr, NULL);
#endif
    vkDestroySurfaceKHR(rvk_ctx.instance, rvk_ctx.surface, NULL);
    vkDestroyInstance(rvk_ctx.instance, NULL);
#ifdef PLATFORM_DESKTOP_GLFW
    rvk_glfw_destroy();
#endif
}

void rvk_instance_init()
{
#ifdef VK_VALIDATION
    rvk_ctx.using_validation = rvk_validation_supported();
#endif

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APP_NAME,
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .pEngineName = "Cool Vulkan Renderer",
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
#ifndef PLATFORM_ANDROID_QUEST
        .apiVersion = VK_API_VERSION_1_3,
#else
        .apiVersion = VK_API_VERSION_1_0, // This should be more dynamic
#endif
    };
    VkInstanceCreateInfo instance_ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };

    uint32_t platform_ext_count = 0;

#ifdef PLATFORM_DESKTOP_GLFW
    const char **platform_exts = glfwGetRequiredInstanceExtensions(&platform_ext_count);
    for (size_t i = 0; i < platform_ext_count; i++)
        rvk_da_append(&rvk_inst_exts, platform_exts[i]);
#else
    rvk_log(RVK_ERROR, "rvk_instance_init() currently requires PLATFORM_DESKTOP_GLFW");
    RVK_EXIT_APP;
#endif
    
#ifdef VK_VALIDATION
    if (rvk_ctx.using_validation) {
        instance_ci.enabledLayerCount = RVK_ARRAY_LEN(rvk_validation_layers);
        instance_ci.ppEnabledLayerNames = rvk_validation_layers;
        rvk_da_append(&rvk_inst_exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
        rvk_populated_debug_msgr_ci(&debug_msgr_ci);
        instance_ci.pNext = &debug_msgr_ci;
    }
#endif

    instance_ci.enabledExtensionCount = rvk_inst_exts.count;
    instance_ci.ppEnabledExtensionNames = rvk_inst_exts.items;

    if (!rvk_inst_exts_satisfied()) RVK_EXIT_APP;

    RAG_VK(vkCreateInstance(&instance_ci, NULL, &rvk_ctx.instance));
}

void rvk_device_init()
{
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queue_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = rvk_ctx.queue_idx,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    VkPhysicalDeviceFeatures features = {
        .samplerAnisotropy = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
    };
    VkDeviceCreateInfo device_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pEnabledFeatures = &features,
        .pQueueCreateInfos = &queue_ci,
        .queueCreateInfoCount = 1,
        .enabledExtensionCount = RVK_ARRAY_LEN(rvk_device_exts),
        .ppEnabledExtensionNames = rvk_device_exts,
    };
#ifdef VK_VALIDATION
    if (rvk_ctx.using_validation) {
        device_ci.enabledLayerCount = RVK_ARRAY_LEN(rvk_validation_layers);
        device_ci.ppEnabledLayerNames = rvk_validation_layers;
    }
#endif

    RAG_VK(vkCreateDevice(rvk_ctx.phys_device, &device_ci, NULL, &rvk_ctx.device));
    vkGetDeviceQueue(rvk_ctx.device, rvk_ctx.queue_idx, 0, &rvk_ctx.unified_queue);
}

void rvk_swapchain_init()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rvk_ctx.phys_device, rvk_ctx.surface, &capabilities);
    rvk_choose_swapchain_fmt();

    uint32_t img_count = RVK_MAX_SWAPCHAIN_IMAGES;
    if (capabilities.maxImageCount) {
        if (RVK_MAX_SWAPCHAIN_IMAGES > capabilities.maxImageCount)
            img_count = capabilities.maxImageCount;
    }
    if (img_count < capabilities.minImageCount) {
        rvk_log(RVK_INFO, "need to increase RVK_MAX_SWAPCHAIN_IMAGES to at least %u", capabilities.minImageCount);
        RVK_EXIT_APP;
    }

    VkSwapchainCreateInfoKHR swapchain_ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = rvk_ctx.surface,
        .minImageCount = img_count,
        .imageFormat = rvk_ctx.surface_fmt.format,
        .imageColorSpace = rvk_ctx.surface_fmt.colorSpace,
        .imageExtent = rvk_ctx.extent = rvk_choose_swp_extent(),
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // I wonder can you combine this with VK_IMAGE_USAGE_SAMPLED_BIT?
        .clipped = VK_TRUE,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = rvk_choose_present_mode(),
        .preTransform = capabilities.currentTransform,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    RAG_VK(vkCreateSwapchainKHR(rvk_ctx.device, &swapchain_ci, NULL, &rvk_ctx.swapchain.handle));
    RAG_VK(vkGetSwapchainImagesKHR(rvk_ctx.device, rvk_ctx.swapchain.handle, &rvk_ctx.swapchain.img_count, NULL));
    RAG_VK(vkGetSwapchainImagesKHR(rvk_ctx.device, rvk_ctx.swapchain.handle, &rvk_ctx.swapchain.img_count, rvk_ctx.swapchain.imgs));
    if (rvk_ctx.swapchain.img_count > RVK_MAX_SWAPCHAIN_IMAGES) {
        rvk_log(RVK_INFO, "need to increase RVK_MAX_SWAPCHAIN_IMAGES to at least %u", rvk_ctx.swapchain.img_count);
        RVK_EXIT_APP;
    }
}

void rvk_img_view_init(Rvk_Image img, VkImageView *img_view)
{
    VkImageViewCreateInfo img_view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = img.format,
        .subresourceRange = {
            .aspectMask = img.aspect_mask,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    RAG_VK(vkCreateImageView(rvk_ctx.device, &img_view_ci, NULL, img_view));
}

void rvk_img_views_init()
{
    for (size_t i = 0; i < rvk_ctx.swapchain.img_count; i++)  {
        Rvk_Image img = {
            .handle = rvk_ctx.swapchain.imgs[i],
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = rvk_ctx.surface_fmt.format,
        };
        rvk_img_view_init(img, &rvk_ctx.swapchain.img_views[i]);
    }
}

void rvk_basic_pl_init(Pipeline_Config config, VkPipeline *pl)
{
    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pName = "main",
        },
    };
    rvk_shader_mod_init(config.vert, &stages[0].module);
    rvk_shader_mod_init(config.frag, &stages[1].module);

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = RVK_ARRAY_LEN(dynamic_states),
        .pDynamicStates = dynamic_states,
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = config.vert_binding_count,
        .pVertexBindingDescriptions = config.vert_bindings,
        .vertexAttributeDescriptionCount = config.vert_attr_count,
        .pVertexAttributeDescriptions = config.vert_attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = config.topology,
    };
    VkViewport viewport = {
        .width    = (float) rvk_ctx.extent.width,
        .height   = (float) rvk_ctx.extent.height,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {.extent = rvk_ctx.extent};
    VkPipelineViewportStateCreateInfo viewport_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = config.polygon_mode,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
    };
    VkPipelineMultisampleStateCreateInfo multisampling_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState color_blend = {
        .colorWriteMask = 0xf, // rgba
        .blendEnable = VK_TRUE,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    };
    VkPipelineColorBlendStateCreateInfo color_blend_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend,
        .logicOp = VK_LOGIC_OP_COPY,
    };
    if (!config.pl_layout) {
        rvk_log(RVK_ERROR, "pipeline layout was NULL");
        RVK_EXIT_APP;
    }
    VkPipelineDepthStencilStateCreateInfo depth_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .maxDepthBounds = 1.0f,
    };
    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = RVK_ARRAY_LEN(stages),
        .pStages = stages,
        .pVertexInputState = &vertex_input_ci,
        .pInputAssemblyState = &input_assembly_ci,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterizer_ci,
        .pMultisampleState = &multisampling_ci,
        .pColorBlendState = &color_blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .pDepthStencilState = &depth_ci,
        .layout = config.pl_layout,
        .renderPass = (config.render_pass) ? config.render_pass : rvk_ctx.render_pass,
    };
    RAG_VK(vkCreateGraphicsPipelines(rvk_ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, pl));
    vkDestroyShaderModule(rvk_ctx.device, stages[0].module, NULL);
    vkDestroyShaderModule(rvk_ctx.device, stages[1].module, NULL);
}

void rvk_sst_pl_init(VkPipelineLayout pl_layout, VkPipeline *pl)
{
    /* setup shader stages */
    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pName = "main",
        },
    };
    rvk_shader_mod_init("./shaders/sst.vert.glsl.spv", &stages[0].module);
    rvk_shader_mod_init("./shaders/sst.frag.glsl.spv", &stages[1].module);

    /* populate fields for graphics pipeline create info */
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = RVK_ARRAY_LEN(dynamic_states),
        .pDynamicStates = dynamic_states,
    };
    VkPipelineVertexInputStateCreateInfo empty_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkPipelineViewportStateCreateInfo viewport_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_FRONT_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    };
    VkPipelineMultisampleStateCreateInfo multisampling_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState color_blend = {
        .colorWriteMask = 0xf, // rgba
        .blendEnable = VK_FALSE,
    };
    VkPipelineColorBlendStateCreateInfo color_blend_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend,
    };
    VkPipelineDepthStencilStateCreateInfo depth_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .maxDepthBounds = 1.0f,
    };
    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = RVK_ARRAY_LEN(stages),
        .pStages = stages,
        .pVertexInputState = &empty_input_state,
        .pInputAssemblyState = &input_assembly_ci,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterizer_ci,
        .pMultisampleState = &multisampling_ci,
        .pColorBlendState = &color_blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .pDepthStencilState = &depth_ci,
        .layout = pl_layout,
        .renderPass = rvk_ctx.render_pass,
        .subpass = 0,
    };

    RAG_VK(vkCreateGraphicsPipelines(rvk_ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, pl));
    vkDestroyShaderModule(rvk_ctx.device, stages[0].module, NULL);
    vkDestroyShaderModule(rvk_ctx.device, stages[1].module, NULL);
}

bool rvk_pl_layout_init(VkPipelineLayoutCreateInfo ci, VkPipelineLayout *pl_layout)
{
    if (!RVK_SUCCEEDED(vkCreatePipelineLayout(rvk_ctx.device, &ci, NULL, pl_layout))) {
        rvk_log(RVK_ERROR, "failed to create pipeline layout");
        return false;
    }

    return true;
}

void rvk_compute_pl_init(const char *shader_name, VkPipelineLayout pl_layout, VkPipeline *pipeline)
{
    VkPipelineShaderStageCreateInfo shader_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .pName = "main",
    };
    rvk_shader_mod_init(shader_name, &shader_ci.module);
    VkComputePipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = pl_layout,
        .stage = shader_ci,
    };
    RAG_VK(vkCreateComputePipelines(rvk_ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, pipeline));
    vkDestroyShaderModule(rvk_ctx.device, shader_ci.module, NULL);
}

void rvk_destroy_pl_res(VkPipeline pipeline, VkPipelineLayout pl_layout)
{
    vkDestroyPipeline(rvk_ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(rvk_ctx.device, pl_layout, NULL);
}

VkCommandBuffer rvk_get_cmd_buff()
{
    return rvk_ctx.cmd_buff;
}

void rvk_reset_pool(VkDescriptorPool pool)
{
    RAG_VK(vkResetDescriptorPool(rvk_ctx.device, pool, 0));
}

void rvk_shader_mod_init(const char *file_name, VkShaderModule *module)
{
#ifdef PLATFORM_ANDROID_QUEST
    char *content = NULL;
    if (!rvk_aam) {
        rvk_log(RVK_ERROR, "set android asset manager with rvk_set_android_asset_man(AAssetManager *aam)");
        RVK_EXIT_APP;
    }
    AAsset *file  = AAssetManager_open(rvk_aam, file_name, AASSET_MODE_BUFFER);
    off_t length  = AAsset_getLength(file);
    content = malloc(length);
    AAsset_read(file, content, length);
    VkShaderModuleCreateInfo module_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = length,
        .pCode = (uint32_t *)content,
    };
    RAG_VK(vkCreateShaderModule(rvk_ctx.device, &module_ci, NULL, module));
    free(content);
#endif // PLATFORM_ANDROID_QUEST

#ifdef PLATFORM_DESKTOP_GLFW
    Rvk_String_Builder sb = {0};
    if (!rvk_read_entire_file(file_name, &sb)) {
        rvk_log(RVK_ERROR, "failed to read entire file %s", file_name);
        RVK_EXIT_APP;
    }
    VkShaderModuleCreateInfo module_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sb.count,
        .pCode = (const uint32_t *)sb.items,
    };
    RAG_VK(vkCreateShaderModule(rvk_ctx.device, &module_ci, NULL, module));
    rvk_sb_free(sb);
#endif // PLATFORM_DESKTOP_GLFW
}

void rvk_render_pass_init()
{
    VkAttachmentDescription color = {
        .format = rvk_ctx.surface_fmt.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_ref = {
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentDescription depth = {
        .format = VK_FORMAT_D32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription gfx_subpass = {
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };
    VkAttachmentDescription attachments[] = {color, depth};
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo render_pass_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = RVK_ARRAY_LEN(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &gfx_subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    RAG_VK(vkCreateRenderPass(rvk_ctx.device, &render_pass_ci, NULL, &rvk_ctx.render_pass));
}

VkFormat rvk_surface_fmt()
{
    return rvk_ctx.surface_fmt.format;
}

void rvk_create_render_pass(VkRenderPassCreateInfo *rp_create_info, VkRenderPass *render_pass)
{
    RAG_VK(vkCreateRenderPass(rvk_ctx.device, rp_create_info, NULL, render_pass));
}

void rvk_destroy_render_pass(VkRenderPass render_pass)
{
    vkDestroyRenderPass(rvk_ctx.device, render_pass, NULL);
}

void rvk_frame_buffs_init()
{
    for (size_t i = 0; i < rvk_ctx.swapchain.img_count; i++) {
        VkImageView attachments[] = {rvk_ctx.swapchain.img_views[i], rvk_ctx.depth_img_view};
        VkFramebufferCreateInfo frame_buff_ci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = rvk_ctx.render_pass,
            .attachmentCount = RVK_ARRAY_LEN(attachments),
            .pAttachments = attachments,
            .width =  rvk_ctx.extent.width,
            .height = rvk_ctx.extent.height,
            .layers = 1,
        };
        RAG_VK(vkCreateFramebuffer(rvk_ctx.device, &frame_buff_ci, NULL, &rvk_ctx.swapchain.frame_buffs[i]));
    }
}

bool rvk_create_frame_buff(uint32_t w, uint32_t h, VkImageView *atts, uint32_t att_count, VkRenderPass rp, VkFramebuffer *fb)
{
    VkFramebufferCreateInfo frame_buff_ci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = rp,
        .attachmentCount = att_count,
        .pAttachments = atts,
        .width =  w,
        .height = h,
        .layers = 1,
    };
    if (!RVK_SUCCEEDED(vkCreateFramebuffer(rvk_ctx.device, &frame_buff_ci, NULL, fb))) {
        rvk_log(RVK_ERROR, "failed to create frame buffer");
        return false;
    }

    return true;
}

void rvk_destroy_frame_buff(VkFramebuffer frame_buff)
{
    vkDestroyFramebuffer(rvk_ctx.device, frame_buff, NULL);
}

void rvk_begin_render_pass(float r, float g, float b, float a)
{
    VkClearValue clear_color = {
        .color = {{r, g, b, a}}
    };
    VkClearValue clear_depth = {
        .depthStencil = {
            .depth = 1.0f,
            .stencil = 0,
        }
    };
    VkClearValue clear_values[] = {clear_color, clear_depth};
    VkRenderPassBeginInfo begin_rp = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = rvk_ctx.render_pass,
        .framebuffer = rvk_ctx.swapchain.frame_buffs[rvk_img_idx],
        .renderArea.extent = rvk_ctx.extent,
        .clearValueCount = RVK_ARRAY_LEN(clear_values),
        .pClearValues = clear_values,
    };
    vkCmdBeginRenderPass(rvk_ctx.cmd_buff, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
}

bool rvk_cmd_begin_render_pass_(VkCommandBuffer cmd_buff, Rvk_Render_Pass_Begin_Info rvk_bi)
{
    VkClearValue clear_color = {
        .color = {
            .float32 = {
                0.0f/255.0f, 228.0f/255.0f, 48/255.0f, 255/255.0f
            }
        }
    };
    VkClearValue clear_depth = {.depthStencil = { .depth = 1.0f}};
    VkClearValue clear_values[] = {clear_color, clear_depth};
    VkRenderPassBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass        = (rvk_bi.render_pass       ) ? rvk_bi.render_pass        : rvk_ctx.render_pass,
    bi.framebuffer       = (rvk_bi.framebuffer       ) ? rvk_bi.framebuffer        : rvk_ctx.swapchain.frame_buffs[rvk_img_idx],
    bi.clearValueCount   = (rvk_bi.clear_value_count ) ? rvk_bi.clear_value_count  : RVK_ARRAY_LEN(clear_values),
    bi.pClearValues      = (rvk_bi.p_clear_values    ) ? rvk_bi.p_clear_values     : clear_values,
    bi.renderArea.extent = rvk_ctx.extent;
    if (rvk_bi.render_area.extent.width)  bi.renderArea.extent.width  = rvk_bi.render_area.extent.width;
    if (rvk_bi.render_area.extent.height) bi.renderArea.extent.height = rvk_bi.render_area.extent.height;
    if (bi.renderArea.extent.width == 0 || bi.renderArea.extent.height == 0) {
        rvk_log(RVK_ERROR, "extent was not specified for render pass");
        return false;
    }
    vkCmdBeginRenderPass(cmd_buff, &bi, VK_SUBPASS_CONTENTS_INLINE);

    return true;
}

void rvk_begin_offscreen_render_pass(float r, float g, float b, float a, VkRenderPass rp, VkFramebuffer fb, VkExtent2D extent)
{
    VkClearValue clear_color = {
        .color = {
            {r, g, b, a}
        }
    };
    VkClearValue clear_depth = {
        .depthStencil = {
            .depth = 1.0f,
            .stencil = 0,
        }
    };
    VkClearValue clear_values[] = {clear_color, clear_depth};
    VkRenderPassBeginInfo begin_rp = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = rp,
        .framebuffer = fb,
        .renderArea.extent = extent,
        .clearValueCount = RVK_ARRAY_LEN(clear_values),
        .pClearValues = clear_values,
    };
    vkCmdBeginRenderPass(rvk_ctx.cmd_buff, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
}

void rvk_end_render_pass()
{
    vkCmdEndRenderPass(rvk_ctx.cmd_buff);
}

void rvk_cmd_end_render_pass(VkCommandBuffer cmd_buff)
{
    vkCmdEndRenderPass(cmd_buff);
}

void rvk_end_rec_gfx()
{
    RAG_VK(vkEndCommandBuffer(rvk_ctx.cmd_buff));
}

void rvk_end_command_buffer(VkCommandBuffer cmd_buff)
{
    RAG_VK(vkEndCommandBuffer(cmd_buff));
}

void rvk_submit_gfx()
{
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &rvk_ctx.cmd_buff,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &rvk_ctx.render_fin_sem,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &rvk_ctx.img_avail_sem,
        .pWaitDstStageMask = &wait_stage,
    };

    RAG_VK(vkQueueSubmit(rvk_ctx.unified_queue, 1, &submit, rvk_ctx.fence));

    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &rvk_ctx.render_fin_sem,
        .swapchainCount = 1,
        .pSwapchains = &rvk_ctx.swapchain.handle,
        .pImageIndices = &rvk_img_idx,
    };
    VkResult res = vkQueuePresentKHR(rvk_ctx.unified_queue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || rvk_ctx.swapchain.buff_resized) {
        rvk_ctx.swapchain.buff_resized = false;
        rvk_recreate_swapchain();
    } else if (!RVK_SUCCEEDED(res)) {
        rvk_handle_bad_vk_result(res, "vkQueuePresentKHR");
    }
}

void rvk_queue_submit_(VkQueue queue, VkFence fence, Rvk_Submit_Info rvk_si)
{
    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext                = (rvk_si.p_next                ) ? rvk_si.p_next                 : NULL;
    si.waitSemaphoreCount   = (rvk_si.wait_semaphore_count  ) ? rvk_si.wait_semaphore_count   : 0;
    si.pWaitSemaphores      = (rvk_si.p_wait_semaphores     ) ? rvk_si.p_wait_semaphores      : NULL;
    si.pWaitDstStageMask    = (rvk_si.p_wait_dst_stage_mask ) ? rvk_si.p_wait_dst_stage_mask  : NULL;
    si.commandBufferCount   = (rvk_si.command_buffer_count  ) ? rvk_si.command_buffer_count   : 1;
    si.pCommandBuffers      = rvk_si.p_command_buffers;
    si.signalSemaphoreCount = (rvk_si.signal_semaphore_count) ? rvk_si.signal_semaphore_count : 0;
    si.pSignalSemaphores    = (rvk_si.p_signal_semaphores   ) ? rvk_si.p_signal_semaphores    : NULL;
    if (!rvk_si.p_command_buffers) {
        rvk_log(RVK_ERROR, "command buffers must be specified in Rvk_Submit_Info");
        RVK_EXIT_APP;
    }
    RAG_VK(vkQueueSubmit(queue, 1, &si, fence));
}

void rvk_draw(VkPipeline pl, VkPipelineLayout pl_layout, Rvk_Buffer vtx_buff, Rvk_Buffer idx_buff, void *float16_mvp)
{
    RVK_ASSERT(0 && "rvk_draw deprecated");

    VkCommandBuffer cmd_buffer = rvk_ctx.cmd_buff;
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    VkViewport viewport = {
        .width = (float)rvk_ctx.extent.width,
        .height =(float)rvk_ctx.extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {.extent = rvk_ctx.extent};
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vtx_buff.handle, offsets);
    vkCmdBindIndexBuffer(cmd_buffer, idx_buff.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(cmd_buffer, pl_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, float16_mvp);
    vkCmdDrawIndexed(cmd_buffer, idx_buff.count, 1, 0, 0, 0);
}

void rvk_bind_gfx(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet *ds, size_t ds_count)
{
    VkCommandBuffer cmd_buff = rvk_ctx.cmd_buff;

    vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    VkViewport viewport = {
        .width    = rvk_ctx.extent.width,
        .height   = rvk_ctx.extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd_buff, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = rvk_ctx.extent;
    vkCmdSetScissor(cmd_buff, 0, 1, &scissor);

    /* bind descriptor sets */
    for (size_t i = 0; i < ds_count; i++)
        vkCmdBindDescriptorSets(cmd_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl_layout, i, 1, &ds[i], 0, NULL);
}

void rvk_bind_gfx_extent(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet *ds, size_t ds_count, VkExtent2D extent)
{
    VkCommandBuffer cmd_buff = rvk_ctx.cmd_buff;

    vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    VkViewport viewport = {
        .width    = extent.width,
        .height   = extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd_buff, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd_buff, 0, 1, &scissor);

    /* bind descriptor sets */
    for (size_t i = 0; i < ds_count; i++)
        vkCmdBindDescriptorSets(cmd_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl_layout, i, 1, &ds[i], 0, NULL);
}

void rvk_draw_buffers(Rvk_Buffer vtx_buff, Rvk_Buffer idx_buff)
{
    VkCommandBuffer cmd_buff = rvk_ctx.cmd_buff;
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buff, 0, 1, &vtx_buff.handle, offsets);
    vkCmdBindIndexBuffer(cmd_buff, idx_buff.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buff, idx_buff.count, 1, 0, 0, 0);
}

void rvk_dispatch(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds, size_t x, size_t y, size_t z)
{
    vkCmdBindPipeline(rvk_ctx.cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, pl);
    vkCmdBindDescriptorSets(rvk_ctx.cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, pl_layout, 0, 1, &ds, 0, NULL);
    vkCmdDispatch(rvk_ctx.cmd_buff, x, y, z);
}

void rvk_push_const(VkPipelineLayout pl_layout, VkShaderStageFlags flags, uint32_t size, void *value)
{
    VkCommandBuffer cmd_buff = rvk_ctx.cmd_buff;
    vkCmdPushConstants(cmd_buff, pl_layout, flags, 0, size, value);
}

void rvk_draw_sst(VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet ds)
{
    vkCmdBindPipeline(rvk_ctx.cmd_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    vkCmdBindDescriptorSets(rvk_ctx.cmd_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl_layout, 0, 1, &ds, 0, NULL);

    VkViewport viewport = {
        .width = (float)rvk_ctx.extent.width,
        .height =(float)rvk_ctx.extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(rvk_ctx.cmd_buff, 0, 1, &viewport);
    VkRect2D scissor = { .extent = rvk_ctx.extent, };
    vkCmdSetScissor(rvk_ctx.cmd_buff, 0, 1, &scissor);
    vkCmdDraw(rvk_ctx.cmd_buff, 3, 1, 0, 0);
}

#ifdef PLATFORM_DESKTOP_GLFW
void rvk_compute_pl_barrier()
{
    VkMemoryBarrier2KHR barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR
    };

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(rvk_ctx.cmd_buff, &dependency);
}
#endif // PLATFORM_DESKTOP_GLFW

void rvk_draw_points(Rvk_Buffer vtx_buff, void *float16_mvp, VkPipeline pl, VkPipelineLayout pl_layout, VkDescriptorSet *ds_sets, size_t ds_set_count)
{
    VkCommandBuffer cmd_buffer = rvk_ctx.cmd_buff;
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    VkViewport viewport = {
        .width = (float)rvk_ctx.extent.width,
        .height =(float)rvk_ctx.extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {.extent = rvk_ctx.extent};
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vtx_buff.handle, offsets);

    for (size_t i = 0; i < ds_set_count; i++) {
        vkCmdBindDescriptorSets(
            cmd_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pl_layout, i, 1, &ds_sets[i], 0, NULL
        );
    }

    vkCmdPushConstants(cmd_buffer, pl_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, float16_mvp);
    vkCmdDraw(cmd_buffer, vtx_buff.count, 1, 0, 0);
}

void rvk_wait_to_begin_gfx()
{
    RAG_VK(vkWaitForFences(rvk_ctx.device, 1, &rvk_ctx.fence, VK_TRUE, UINT64_MAX));

    VkResult res = vkAcquireNextImageKHR(
        rvk_ctx.device, rvk_ctx.swapchain.handle, UINT64_MAX,
        rvk_ctx.img_avail_sem, VK_NULL_HANDLE, &rvk_img_idx
    );
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        rvk_swapchain_init();
    } else if (!RVK_SUCCEEDED(res) && res != VK_SUBOPTIMAL_KHR) {
        rvk_log(RVK_ERROR, "failed to acquire swapchain image");
        RVK_EXIT_APP;
    } else if (res == VK_SUBOPTIMAL_KHR) {
        rvk_log(RVK_WARNING, "suboptimal swapchain image");
    }

    RAG_VK(vkResetFences(rvk_ctx.device, 1, &rvk_ctx.fence));
    RAG_VK(vkResetCommandBuffer(rvk_ctx.cmd_buff, 0));
}

void rvk_wait_reset()
{
    RAG_VK(vkWaitForFences(rvk_ctx.device, 1, &rvk_ctx.fence, VK_TRUE, UINT64_MAX));
    RAG_VK(vkResetFences(rvk_ctx.device, 1, &rvk_ctx.fence));
    RAG_VK(vkResetCommandBuffer(rvk_ctx.cmd_buff, 0));
}

void rvk_wait_for_fences(VkFence *fences, uint32_t fence_count)
{
    RAG_VK(vkWaitForFences(rvk_ctx.device, fence_count, fences, VK_TRUE, UINT64_MAX));
}

void rvk_reset_fences(VkFence *fences, uint32_t fence_count)
{
    RAG_VK(vkResetFences(rvk_ctx.device, fence_count, fences));
}

void rvk_reset_command_buffer(VkCommandBuffer cmd_buff)
{
    RAG_VK(vkResetCommandBuffer(cmd_buff, 0));
}

void rvk_begin_command_buffer(VkCommandBuffer cmd_buff)
{
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, };
    RAG_VK(vkBeginCommandBuffer(cmd_buff, &begin_info));
}

void rvk_begin_rec_gfx()
{
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, };
    RAG_VK(vkBeginCommandBuffer(rvk_ctx.cmd_buff, &begin_info));
}

void rvk_recreate_swapchain()
{
#ifdef PLATFORM_DESKTOP_GLFW
    rvk_glfw_wait_resize_frame_buffer();
#else
    rvk_log(RVK_ERROR, "rvk_recreate_swapchain currently only supports PLATFORM_DESKTOP_GLFW");
    RVK_APP_FAIL;
#endif

    vkDeviceWaitIdle(rvk_ctx.device);
    rvk_destroy_swapchain();
    rvk_swapchain_init();
    rvk_img_views_init();
    rvk_depth_init();
    rvk_frame_buffs_init();
}

void rvk_depth_init()
{
    rvk_ctx.depth_img.extent = rvk_ctx.extent;
    rvk_ctx.depth_img.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
    rvk_ctx.depth_img.format = VK_FORMAT_D32_SFLOAT;
    rvk_img_init(
        &rvk_ctx.depth_img,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    rvk_img_view_init(rvk_ctx.depth_img, &rvk_ctx.depth_img_view);
}

bool rvk_uniform_buff_init(size_t size, void *data, Rvk_Buffer *buffer)
{
    return rvk_buff_init(
        size,
        1, // 1 uniform buffer
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        RVK_BUFFER_TYPE_UNIFORM,
        data,
        buffer
    );
}

bool rvk_is_device_suitable(VkPhysicalDevice phys_device)
{
    if (!rvk_has_unified_gfx_and_present_queue(phys_device)) return false;

    VkPhysicalDeviceProperties props = {0};
    VkPhysicalDeviceFeatures features = {0};
    vkGetPhysicalDeviceProperties(phys_device, &props);
    vkGetPhysicalDeviceFeatures(phys_device, &features);
    if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        props.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        rvk_log(RVK_ERROR, "device not suitable, neither discrete nor integrated GPU present");
        return false;
    };
    if (!features.geometryShader) {
        rvk_log(RVK_ERROR, "device not suitable, geometry shader not present");
        return false;
    }
    if (!rvk_device_exts_supported(phys_device)) return false;
    if (!rvk_swapchain_adequate(phys_device))    return false;

    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL rvk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data)
{
    (void)msg_type;
    (void)p_user_data;
    Rvk_Log_Level log_lvl = translate_msg_severity(msg_severity);
    if (log_lvl < MIN_SEVERITY) return VK_FALSE;
    rvk_log(log_lvl, "%s", p_callback_data->pMessage);
    return VK_FALSE;
}

Rvk_Log_Level rvk_report_flag_to_vk_log(VkDebugReportFlagsEXT flags)
{
    if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT || flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        return RVK_INFO;

    if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT || flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        return RVK_WARNING;

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        return RVK_ERROR;

    RVK_ASSERT(0 && "this message severity is not handled");

    return RVK_ERROR; // unreachable
}

VKAPI_ATTR VkBool32 VKAPI_CALL rvk_debug_report_callback(VkDebugReportFlagsEXT flags,
                                                     VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                                     size_t location, int32_t messageCode,
                                                     const char *pLayerPrefix,
                                                     const char *pMessage, void *pUserData)
{
    (void)objectType;
    (void)object;
    (void)location;
    (void)messageCode;
    (void)pUserData;
    (void)pLayerPrefix;
    Rvk_Log_Level log_lvl = rvk_report_flag_to_vk_log(flags);
    if (log_lvl < MIN_SEVERITY) return VK_FALSE;
    rvk_log(log_lvl, "[Vulkan Validation] %s", pMessage);
    return VK_FALSE;
}

void rvk_populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *debug_msgr_ci)
{
    debug_msgr_ci->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_msgr_ci->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_msgr_ci->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_msgr_ci->pfnUserCallback = rvk_debug_callback;
}

Rvk_Log_Level translate_msg_severity(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity)
{
    switch (msg_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return RVK_INFO;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return RVK_INFO;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return RVK_WARNING;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return RVK_ERROR;
    default: RVK_ASSERT(0 && "this message severity is not handled");
    }

    return RVK_ERROR; // unreachable
}

void rvk_setup_debug_msgr()
{
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
    rvk_populated_debug_msgr_ci(&debug_msgr_ci);
    RVK_LOAD_PFN(vkCreateDebugUtilsMessengerEXT);
    if (vkCreateDebugUtilsMessengerEXT) {
        RAG_VK(vkCreateDebugUtilsMessengerEXT(rvk_ctx.instance, &debug_msgr_ci, NULL, &rvk_ctx.debug_msgr));
    } else {
        rvk_log(RVK_ERROR, "failed to load function pointer for vkCreateDebugUtilesMessenger");
    }
}

bool rvk_setup_report_callback()
{
    VkDebugReportCallbackCreateInfoEXT report_callback_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                 VK_DEBUG_REPORT_WARNING_BIT_EXT |
                 VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                 VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                 VK_DEBUG_REPORT_DEBUG_BIT_EXT,
        .pfnCallback = rvk_debug_report_callback,
    };
    RVK_LOAD_PFN(vkCreateDebugReportCallbackEXT);
    if (vkCreateDebugReportCallbackEXT) {
        return RVK_SUCCEEDED(vkCreateDebugReportCallbackEXT(rvk_ctx.instance, &report_callback_ci, NULL, &rvk_ctx.report_callback));
    } else {
        rvk_log(RVK_ERROR, "failed to load function pointer for vkCreateDebugReportCallbackEXT");
        return false;
    }
}

uint32_t rvk_get_unified_gfx_and_present_queue_idx(VkPhysicalDevice phys_device)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fam_props[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, queue_fam_props);
    bool has_gfx = false;
    for (uint32_t i = 0; i < queue_fam_count; i++) {
        /* graphics support? */
        if (queue_fam_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            has_gfx = true;

        /* present support? */
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, rvk_ctx.surface, &present_support);
        if (present_support && has_gfx) return i;
        else has_gfx = false;
    }

    rvk_log(RVK_ERROR, "If you don't want to exit, next time call rvk_has_unified_gfx_and_present_queue() first");
    RVK_EXIT_APP;
}

bool rvk_has_unified_gfx_and_present_queue(VkPhysicalDevice phys_device)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fam_props[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, queue_fam_props);
    bool has_gfx = false;
    for (uint32_t i = 0; i < queue_fam_count; i++) {
        /* graphics support? */
        if (queue_fam_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            has_gfx = true;

        /* present support? */
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, rvk_ctx.surface, &present_support);
        if (present_support && has_gfx) return true;
        else has_gfx = false;
    }

    return false;
}

bool rvk_set_gfx_capable_queue_idx(VkPhysicalDevice phys_device)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fam_props[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, queue_fam_props);
    bool has_gfx = false;
    for (size_t i = 0; i < queue_fam_count; i++) {
        if (queue_fam_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            rvk_ctx.queue_idx = i;
            has_gfx = true;
        }
        if (has_gfx) return true;
    }

    rvk_log(RVK_ERROR, "missing graphics queue");
    return false;
}

void rvk_pick_phys_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(rvk_ctx.instance, &device_count, NULL);
    VkPhysicalDevice phys_devices[device_count];
    vkEnumeratePhysicalDevices(rvk_ctx.instance, &device_count, phys_devices);
    /* pick the first physical devie with graphics and present support */
    for (size_t i = 0; i < device_count; i++) {
        if (rvk_is_device_suitable(phys_devices[i])) {
            rvk_ctx.phys_device = phys_devices[i];
            rvk_ctx.queue_idx = rvk_get_unified_gfx_and_present_queue_idx(rvk_ctx.phys_device);
            return;
        }
    }

    rvk_log(RVK_ERROR, "missing unified queue with graphics and present");
    RVK_EXIT_APP;
}

bool rvk_swapchain_adequate(VkPhysicalDevice phys_device)
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, rvk_ctx.surface, &surface_fmt_count, NULL);
    if (!surface_fmt_count) {
        rvk_log(RVK_ERROR, "swapchain inadequate because surface format count was zero");
        return false;
    }
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, rvk_ctx.surface, &present_mode_count, NULL);
    if (!present_mode_count) {
        rvk_log(RVK_ERROR, "swapchain inadequate because present mode count was zero");
        return false;
    }

    return true;
}

void rvk_choose_swapchain_fmt()
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(rvk_ctx.phys_device, rvk_ctx.surface, &surface_fmt_count, NULL);
    VkSurfaceFormatKHR fmts[surface_fmt_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(rvk_ctx.phys_device, rvk_ctx.surface, &surface_fmt_count, fmts);
    for (size_t i = 0; i < surface_fmt_count; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            rvk_ctx.surface_fmt = fmts[i];
            return;
        }
    }

    if (surface_fmt_count) {
        rvk_log(RVK_WARNING, "default surface format %d and colorspace %d", fmts[0].format, fmts[0].colorSpace);
        rvk_ctx.surface_fmt = fmts[0];
        return;
    } else {
        rvk_log(RVK_ERROR, "failed to find any surface swapchain formats");
        rvk_log(RVK_ERROR, "this shouldn't have happened if swapchain_adequate returned true");
        RVK_EXIT_APP;
    }
}

VkPresentModeKHR rvk_choose_present_mode()
{
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(rvk_ctx.phys_device, rvk_ctx.surface, &present_mode_count, NULL);
    VkPresentModeKHR present_modes[present_mode_count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(rvk_ctx.phys_device, rvk_ctx.surface, &present_mode_count, present_modes);
    for (size_t i = 0; i < present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_modes[i];
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D rvk_choose_swp_extent()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rvk_ctx.phys_device, rvk_ctx.surface, &capabilities);
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
#ifdef PLATFORM_DESKTOP_GLFW
        glfwGetFramebufferSize(rvk_glfw_window, &width, &height);
#endif
        RVK_ASSERT(width && height);
        VkExtent2D extent = {
            .width = width,
            .height = height
        };

        extent.width = clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

void rvk_destroy_swapchain()
{
    vkDestroyImageView(rvk_ctx.device, rvk_ctx.depth_img_view, NULL);
    vkDestroyImage(rvk_ctx.device, rvk_ctx.depth_img.handle, NULL);
    vkFreeMemory(rvk_ctx.device, rvk_ctx.depth_img.mem, NULL);

    for (size_t i = 0; i < rvk_ctx.swapchain.img_count; i++) {
        vkDestroyFramebuffer(rvk_ctx.device, rvk_ctx.swapchain.frame_buffs[i], NULL);
        vkDestroyImageView(rvk_ctx.device, rvk_ctx.swapchain.img_views[i], NULL);
    }
    vkDestroySwapchainKHR(rvk_ctx.device, rvk_ctx.swapchain.handle, NULL);

    /* reset image count, otherwise call to vkGetSwapchainImagesKHR will fail as a query */
    rvk_ctx.swapchain.img_count = 0;
}

bool rvk_find_mem_type_idx(uint32_t type, VkMemoryPropertyFlags properties, uint32_t *idx)
{
    VkPhysicalDeviceMemoryProperties mem_properites = {0};
    vkGetPhysicalDeviceMemoryProperties(rvk_ctx.phys_device, &mem_properites);
    for (uint32_t i = 0; i < mem_properites.memoryTypeCount; i++) {
        if (type & (1 << i) && (mem_properites.memoryTypes[i].propertyFlags & properties) == properties) {
            *idx = i;
            return true;
        }
    }

    return false;
}

bool rvk_inst_exts_satisfied()
{
    uint32_t avail_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, avail_exts);
    size_t unsatisfied_exts = rvk_inst_exts.count;
    for (size_t i = 0; i < rvk_inst_exts.count; i++) {
        bool found = false;
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(rvk_inst_exts.items[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
                found = true;
                break;
            }
        }
        if (!found)
            rvk_log(RVK_ERROR, "instance extension `%s` not available", rvk_inst_exts.items[i]);
    }

    return false;
}

bool rvk_validation_supported()
{
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties avail_layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, avail_layers);
    size_t unsatisfied_layers = RVK_ARRAY_LEN(rvk_validation_layers);
    for (size_t i = 0; i < RVK_ARRAY_LEN(rvk_validation_layers); i++) {
        bool found = false;
        for (size_t j = 0; j < layer_count; j++) {
            if (strcmp(rvk_validation_layers[i], avail_layers[j].layerName) == 0) {
                if (--unsatisfied_layers == 0)
                    return true;
                found = true;
                break;
            }
        }
        if (!found)
            rvk_log(RVK_ERROR, "validation layer `%s` not available", rvk_validation_layers[i]);
    }

    return true;
}

bool rvk_device_exts_supported(VkPhysicalDevice phys_device)
{
    uint32_t avail_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateDeviceExtensionProperties(phys_device, NULL, &avail_ext_count, avail_exts);
    uint32_t unsatisfied_exts = RVK_ARRAY_LEN(rvk_device_exts); 
    for (size_t i = 0; i < RVK_ARRAY_LEN(rvk_device_exts); i++) {
        bool found = false;
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(rvk_device_exts[i], avail_exts[j].extensionName) == 0) {
                if (--unsatisfied_exts == 0)
                    return true;
                found = true;
                break;
            }
        }
        if (!found)
            rvk_log(RVK_ERROR, "device extension `%s` not available", rvk_device_exts[i]);
    }

    return false;
}

bool rvk_buff_init(size_t size, size_t count, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props, Rvk_Buffer_Type type, void *data, Rvk_Buffer *buffer)
{
    if (!buffer) {
        rvk_log(RVK_ERROR, "buffer was NULL inside of rvk_buff_init");
        return false;
    }

    buffer->size = size;
    buffer->count = count;

    if (!data) {
        rvk_log(RVK_ERROR, "rvk_buff_init failed data was NULL");
        return false;
    }

    buffer->data = data;

    VkBufferCreateInfo buffer_ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = (VkDeviceSize) buffer->size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (!RVK_SUCCEEDED(vkCreateBuffer(rvk_ctx.device, &buffer_ci, NULL, &buffer->handle))) {
        rvk_log(RVK_ERROR, "failed to create buffer");
        return false;
    }

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(rvk_ctx.device, buffer->handle, &mem_reqs);
    VkMemoryAllocateInfo alloc_ci = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
    };
    if (!rvk_find_mem_type_idx(mem_reqs.memoryTypeBits, mem_props, &alloc_ci.memoryTypeIndex)) {
        rvk_log(RVK_ERROR, "while initializing buffer, memory not suitable based on memory requirements");
        return false;
    }
    if (!RVK_SUCCEEDED(vkAllocateMemory(rvk_ctx.device, &alloc_ci, NULL, &buffer->mem))) {
        rvk_log(RVK_ERROR, "failed to allocate buffer memory!");
        return false;
    }
    if (!RVK_SUCCEEDED(vkBindBufferMemory(rvk_ctx.device, buffer->handle, buffer->mem, 0))) {
        rvk_log(RVK_ERROR, "failed to bind buffer memory");
        return false;
    }

    /* book keeping */
    buffer->info.buffer = buffer->handle;
    buffer->info.range  = buffer->size;
    buffer->type = type;

    return true;
}

bool rvk_comp_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer)
{
    return rvk_buff_init(
        size,
        count,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT  |  // in case we want to transfer data back to host
        VK_BUFFER_USAGE_TRANSFER_DST_BIT  |  // usually this buffer is the destination of a staging buffer
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |  // TODO: this seems very example-specific
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,  // compute buffer
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // gpu land
        RVK_BUFFER_TYPE_COMPUTE,             // book keeping
        data,
        buffer
    );
}

bool rvk_vtx_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer)
{
    return rvk_buff_init(
        size,
        count,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        RVK_BUFFER_TYPE_VERTEX,
        data,
        buffer
    );
}

bool rvk_idx_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer)
{
    return rvk_buff_init(
        size,
        count,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        RVK_BUFFER_TYPE_INDEX,
        data,
        buffer
    );
}

void rvk_stage_buff_init(size_t size, size_t count, void *data, Rvk_Buffer *buffer)
{
    rvk_buff_init(
        size,
        count,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        RVK_BUFFER_TYPE_STAGING,
        data,
        buffer
    );
}

bool rvk_buff_map(Rvk_Buffer *buff)
{
    if (!buff || !buff->handle) {
        rvk_log(RVK_ERROR, "rvk_mem_map failed, buffer invalid");
        return false;
    }

    if (!buff->size) {
        rvk_log(RVK_ERROR, "rvk_mem_map failed, buffer size was zero, and that doesn't make sense");
        return false;
    }

    if (!RVK_SUCCEEDED(vkMapMemory(rvk_ctx.device, buff->mem, 0, buff->size, 0, &buff->mapped))) {
        rvk_log(RVK_ERROR, "unable to map memory for staging buffer");
        return false;
    }

    return true;
}

bool rvk_buff_unmap(Rvk_Buffer *buff)
{
    if (!buff || !buff->handle) {
        rvk_log(RVK_ERROR, "rvk_buff_map failed, buffer invalid");
        return false;
    }
    vkUnmapMemory(rvk_ctx.device, buff->mem);
    buff->mapped = NULL;
    return true;
}

void rvk_buff_destroy(Rvk_Buffer buffer)
{
    if (buffer.mapped) {
        rvk_log(RVK_WARNING, "rvk_buff_destroy: buffer type `%s`", rvk_buff_type_as_str(buffer.type));
        rvk_log(RVK_WARNING, "    did you call vkUnmapMemory on VkBuffer %p?", buffer.handle);
        rvk_log(RVK_WARNING, "    either manually buffer->mapped = NULL, or call rvk_buff_unmap()", buffer.handle);
    }

    vkDestroyBuffer(rvk_ctx.device, buffer.handle, NULL);
    vkFreeMemory(rvk_ctx.device, buffer.mem, NULL);
}

const char *rvk_buff_type_as_str(Rvk_Buffer_Type type)
{
    assert(RVK_BUFFER_TYPE_COUNT == 6 && "update buffer types");
    switch (type) {
    case RVK_BUFFER_TYPE_ANY:     return "any";
    case RVK_BUFFER_TYPE_VERTEX:  return "vertex";
    case RVK_BUFFER_TYPE_INDEX:   return "index";
    case RVK_BUFFER_TYPE_COMPUTE: return "compute";
    case RVK_BUFFER_TYPE_UNIFORM: return "uniform";
    case RVK_BUFFER_TYPE_STAGING: return "staging";
    default:                      return "unrecognized";
    }
}

void rvk_buff_staged_upload(Rvk_Buffer buff)
{
    if (!buff.handle) {
        rvk_log(RVK_ERROR, "rvk_buff_staged_upload failed, invalid VkBuffer handle");
        RVK_EXIT_APP;
    }

    Rvk_Buffer stg_buff = {0};
    rvk_stage_buff_init(buff.size, buff.count, buff.data, &stg_buff);
    RAG_VK(vkMapMemory(rvk_ctx.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped));
    memcpy(stg_buff.mapped, buff.data, stg_buff.size);
    vkUnmapMemory(rvk_ctx.device, stg_buff.mem);
    stg_buff.mapped = NULL;

    /* transfer data from staging buffer to vertex buffer */
    rvk_buff_copy(buff, stg_buff, 0);
    rvk_buff_destroy(stg_buff);
}

void rvk_buff_copy(Rvk_Buffer dst_buff, Rvk_Buffer src_buff, VkDeviceSize size)
{
    VkCommandBuffer tmp_cmd_buff = rvk_cmd_quick_begin();
        VkBufferCopy copy_region = {0};
        if (size) {
            copy_region.size = size;
            if (size > dst_buff.size) {
                rvk_log(RVK_ERROR, "Cannot copy buffer, size > dst buffer (won't fit)");
                RVK_EXIT_APP;
            }
            if (size > src_buff.size) {
                rvk_log(RVK_ERROR, "Cannot copy buffer, size > src buffer (cannot copy more than what's available)");
                RVK_EXIT_APP;
            }
        } else {
            // size == 0 means copy the entire src to dst
            if (dst_buff.size < src_buff.size) {
                rvk_log(RVK_ERROR, "Cannot copy buffer, dst buffer < src buffer (won't fit)");
                RVK_EXIT_APP;
            }
            copy_region.size = src_buff.size;
        }
        vkCmdCopyBuffer(tmp_cmd_buff, src_buff.handle, dst_buff.handle, 1, &copy_region);
    rvk_cmd_quick_end(&tmp_cmd_buff);
}

void rvk_img_init(Rvk_Image *img, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    if (img->extent.width == 0 && img->extent.height == 0) {
        rvk_log(RVK_ERROR, "Image must be set with width/height before calling rvk_img_init");
        RVK_EXIT_APP;
    }

    VkImageCreateInfo img_ci = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = img->format,
        .extent      = {img->extent.width, img->extent.height, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    RAG_VK(vkCreateImage(rvk_ctx.device, &img_ci, NULL, &img->handle));

    VkMemoryRequirements mem_reqs = {0};
    vkGetImageMemoryRequirements(rvk_ctx.device, img->handle, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
    };
    if (!rvk_find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex)) {
        rvk_log(RVK_ERROR, "Memory not suitable based on memory requirements");
        RVK_EXIT_APP;
    }
    RAG_VK(vkAllocateMemory(rvk_ctx.device, &alloc_ci, NULL, &img->mem));
    RAG_VK(vkBindImageMemory(rvk_ctx.device, img->handle, img->mem, 0));
}

void rvk_cmd_pool_init()
{
    VkCommandPoolCreateInfo cmd_pool_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = rvk_ctx.queue_idx,
    };
    RAG_VK(vkCreateCommandPool(rvk_ctx.device, &cmd_pool_ci, NULL, &rvk_ctx.pool));
}

bool rvk_allocate_command_buffer_(VkCommandBuffer *buff, Rvk_Command_Buffer_Allocate_Info rvk_ci)
{
    VkCommandBufferAllocateInfo ci = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ci.commandPool        = (rvk_ci.command_pool)         ? rvk_ci.command_pool : rvk_ctx.pool;
    ci.level              = (rvk_ci.level)                ? rvk_ci.level: VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ci.commandBufferCount = (rvk_ci.command_buffer_count) ? rvk_ci.command_buffer_count : 1;
    if (!RVK_SUCCEEDED(vkAllocateCommandBuffers(rvk_ctx.device, &ci, buff))) {
        rvk_log(RVK_ERROR, "failed to create graphics command buffer");
        return false;
    }

    return true;
}

void rvk_cmd_syncs_init()
{
    VkSemaphoreCreateInfo sem_ci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    RAG_VK(vkCreateSemaphore(rvk_ctx.device, &sem_ci, NULL, &rvk_ctx.img_avail_sem));
    RAG_VK(vkCreateSemaphore(rvk_ctx.device, &sem_ci, NULL, &rvk_ctx.render_fin_sem));
    RAG_VK(vkCreateFence(rvk_ctx.device, &fence_ci, NULL, &rvk_ctx.fence));
}

void rvk_create_semaphore(VkSemaphore *semaphore)
{
    VkSemaphoreCreateInfo sem_ci = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    RAG_VK(vkCreateSemaphore(rvk_ctx.device, &sem_ci, NULL, semaphore));
}

void rvk_create_fence(VkFence *fence)
{
    VkFenceCreateInfo fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    RAG_VK(vkCreateFence(rvk_ctx.device, &fence_ci, NULL, fence));
}

void rvk_destroy_semaphore(VkSemaphore semaphore)
{
    vkDestroySemaphore(rvk_ctx.device, semaphore, NULL);
}

void rvk_destroy_fence(VkFence fence)
{
    vkDestroyFence(rvk_ctx.device, fence, NULL);
}

VkCommandBuffer rvk_cmd_quick_begin()
{
    VkCommandBuffer cmd_buff = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = rvk_ctx.pool,
        .commandBufferCount = 1,
    };
    RAG_VK(vkAllocateCommandBuffers(rvk_ctx.device, &ci, &cmd_buff));
    VkCommandBufferBeginInfo cmd_begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    RAG_VK(vkBeginCommandBuffer(cmd_buff, &cmd_begin));
    return cmd_buff;
}

void rvk_cmd_quick_end(VkCommandBuffer *tmp_cmd_buff)
{
    RAG_VK(vkEndCommandBuffer(*tmp_cmd_buff));
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = tmp_cmd_buff,
    };
    RAG_VK(vkQueueSubmit(rvk_ctx.unified_queue, 1, &submit, VK_NULL_HANDLE));
    RAG_VK(vkQueueWaitIdle(rvk_ctx.unified_queue));
    vkFreeCommandBuffers(rvk_ctx.device, rvk_ctx.pool, 1, tmp_cmd_buff);
}

void transition_img_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer tmp_cmd_buff = rvk_cmd_quick_begin();
        VkPipelineStageFlags src_stg_mask;
        VkPipelineStageFlags dst_stg_mask;
        VkAccessFlags src_access_mask;
        VkAccessFlags dst_access_mask;
        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            src_access_mask = 0;
            dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            src_stg_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stg_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                   new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dst_access_mask = VK_ACCESS_SHADER_READ_BIT;
            src_stg_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stg_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL) {
            src_access_mask = 0;
            dst_access_mask = 0;
            src_stg_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dst_stg_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        } else {
            rvk_log(RVK_ERROR, "old_layout %d with new_layout %d not allowed yet", old_layout, new_layout);
            RVK_EXIT_APP;
        }

        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = src_access_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(
            tmp_cmd_buff, src_stg_mask, dst_stg_mask,
            0, 0, NULL, 0, NULL, 1,
            &barrier
        );
    rvk_cmd_quick_end(&tmp_cmd_buff);
}

void rvk_pl_barrier(VkImageMemoryBarrier barrier)
{
    vkCmdPipelineBarrier(
        rvk_ctx.cmd_buff,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_FLAGS_NONE,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

void rvk_raster_sampler_barrier(VkImage img)
{
    VkImageMemoryBarrier barrier = {
       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
       .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
       .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
       .newLayout = VK_IMAGE_LAYOUT_GENERAL,
       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .image = img,
       .subresourceRange = {
           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
           .baseMipLevel = 0,
           .levelCount = 1,
           .baseArrayLayer = 0,
           .layerCount = 1,
       },
    };
    vkCmdPipelineBarrier(
        rvk_ctx.cmd_buff,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_FLAGS_NONE,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

void rvk_depth_img_barrier(VkImage depth_img)
{
    VkImageMemoryBarrier barrier = {
       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
       .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
       .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
       .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .image = depth_img,
       .subresourceRange = {
           .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
           .baseMipLevel = 0,
           .levelCount = 1,
           .baseArrayLayer = 0,
           .layerCount = 1,
       },
    };
    vkCmdPipelineBarrier(
        rvk_ctx.cmd_buff,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_FLAGS_NONE,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

void rvk_color_img_barrier(VkImage color_img)
{
    VkImageMemoryBarrier barrier = {
       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
       .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
       .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .image = color_img,
       .subresourceRange = {
           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
           .baseMipLevel = 0,
           .levelCount = 1,
           .baseArrayLayer = 0,
           .layerCount = 1,
       },
    };
    vkCmdPipelineBarrier(
        rvk_ctx.cmd_buff,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_FLAGS_NONE,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

void rvk_img_copy(VkImage dst_img, VkBuffer src_buff, VkExtent2D extent)
{
    VkCommandBuffer tmp_cmd_buff = rvk_cmd_quick_begin();
        VkBufferImageCopy region = {
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {extent.width, extent.height, 1},
        };
        vkCmdCopyBufferToImage(tmp_cmd_buff, src_buff, dst_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    rvk_cmd_quick_end(&tmp_cmd_buff);
}

int format_to_size(VkFormat fmt)
{
    if (VK_FORMAT_R8G8B8A8_UNORM <= fmt && fmt <= VK_FORMAT_B8G8R8A8_SRGB) {
        return 4;
    } else if (VK_FORMAT_R8_UNORM <= fmt && fmt <= VK_FORMAT_R8_SRGB) {
        return 1;
    } else {
        rvk_log(RVK_WARNING, "unrecognized format %d, returning 4 instead", fmt);
        return 4;
    }
}

bool rvk_sampler_init(VkSampler *sampler)
{
    VkPhysicalDeviceProperties props = {0};
    vkGetPhysicalDeviceProperties(rvk_ctx.phys_device, &props);
    VkSamplerCreateInfo sampler_ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = props.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };
    if (!RVK_SUCCEEDED(vkCreateSampler(rvk_ctx.device, &sampler_ci, NULL, sampler))) {
        rvk_log(RVK_ERROR, "failed to create sampler");
        return false;
    }

    return true;
}

Rvk_Texture rvk_load_texture(void *data, size_t width, size_t height, VkFormat fmt)
{
    Rvk_Texture texture = {0};

    /* create staging buffer for image */
    Rvk_Buffer stg_buff = {0};
    size_t size  = width * height * format_to_size(fmt);
    size_t count = width * height;
    rvk_stage_buff_init(size, count, data, &stg_buff);
    RAG_VK(vkMapMemory(rvk_ctx.device, stg_buff.mem, 0, stg_buff.size, 0, &stg_buff.mapped));
    memcpy(stg_buff.mapped, data, stg_buff.size);
    rvk_buff_unmap(&stg_buff);

    /* create the image */
    Rvk_Image img = {
        .extent  = {width, height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = fmt,
    };
    rvk_img_init(
        &img,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    transition_img_layout(img.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    rvk_img_copy(img.handle, stg_buff.handle, img.extent);
    transition_img_layout(
        img.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    /* create image view */
    VkImageView img_view;
    rvk_img_view_init(img, &img_view);

    /* create sampler */
    VkPhysicalDeviceProperties props = {0};
    vkGetPhysicalDeviceProperties(rvk_ctx.phys_device, &props);
    VkSamplerCreateInfo sampler_ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = props.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };
    VkSampler sampler;
    RAG_VK(vkCreateSampler(rvk_ctx.device, &sampler_ci, NULL, &sampler));

    rvk_buff_destroy(stg_buff);

    texture.view = img_view;
    texture.sampler = sampler;
    texture.img = img;
    texture.info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texture.info.imageView   = img_view;
    texture.info.sampler     = sampler;

    return texture;
}

void rvk_unload_texture(Rvk_Texture texture)
{
    vkDestroySampler(rvk_ctx.device, texture.sampler, NULL);
    vkDestroyImageView(rvk_ctx.device, texture.view, NULL);
    vkDestroyImage(rvk_ctx.device, texture.img.handle, NULL);
    vkFreeMemory(rvk_ctx.device, texture.img.mem, NULL);
}

void rvk_storage_tex_init(Rvk_Texture *texture, VkExtent2D extent)
{
    if (extent.width == 0 && extent.height == 0) {
        rvk_log(RVK_ERROR, "storage image does not have width or height");
        RVK_EXIT_APP;
    }

    /* setup storage image */
    Rvk_Image img = {
        .extent  = extent,
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    };

    rvk_img_init(
        &img,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    transition_img_layout(
        img.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL
    );

    /* here we could also check if the graphics and compute queues are the same
     * if they are not the same then we can make an image memory barrier
     * for now I will skip this */

    /* create image view */
    VkImageView img_view;
    rvk_img_view_init(img, &img_view);

    /* create sampler */
    VkPhysicalDeviceProperties props = {0};
    vkGetPhysicalDeviceProperties(rvk_ctx.phys_device, &props);
    VkSamplerCreateInfo sampler_ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = props.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };
    VkSampler sampler;
    RAG_VK(vkCreateSampler(rvk_ctx.device, &sampler_ci, NULL, &sampler));

    texture->view = img_view;
    texture->sampler = sampler;
    texture->img = img;
    texture->info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    texture->info.sampler     = sampler;
    texture->info.imageView   = img_view;
}

void rvk_create_ds_pool(VkDescriptorPoolCreateInfo pool_ci, VkDescriptorPool *pool)
{
    RAG_VK(vkCreateDescriptorPool(rvk_ctx.device, &pool_ci, NULL, pool));
}

void rvk_descriptor_pool_arena_init(Rvk_Descriptor_Pool_Arena *arena)
{
    VkDescriptorPoolSize pool_sizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER,                .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       .descriptorCount = MAX_DESCRIPTOR_SETS},
    };
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = RVK_ARRAY_LEN(pool_sizes),
        .pPoolSizes    = pool_sizes,
        .maxSets       = MAX_DESCRIPTOR_SETS,
    };
    rvk_create_ds_pool(pool_ci, &arena->pool);
}

void rvk_descriptor_pool_arena_reset(Rvk_Descriptor_Pool_Arena *arena)
{
    for (size_t i = 0; i < RVK_DESCRIPTOR_TYPE_COUNT; i++)
        arena->pools_usage[i] = 0;
    rvk_reset_pool(arena->pool);
}

void rvk_ds_layout_init(VkDescriptorSetLayoutBinding *bindings, size_t b_count, Rvk_Descriptor_Set_Layout *layout)
{
    for (size_t i = 0; i < b_count; i++) {
        VkDescriptorSetLayoutBinding binding = bindings[i];
        layout->desc_count[binding.descriptorType] += binding.descriptorCount;
    }
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = b_count,
        .pBindings = bindings,
    };
    RAG_VK(vkCreateDescriptorSetLayout(rvk_ctx.device, &layout_ci, NULL, &layout->handle));
}

bool rvk_descriptor_pool_arena_alloc_set(Rvk_Descriptor_Pool_Arena *arena, Rvk_Descriptor_Set_Layout *ds_layout, VkDescriptorSet *set)
{
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = arena->pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &ds_layout->handle,
    };
    for (size_t i = 0; i < RVK_DESCRIPTOR_TYPE_COUNT; i++)
        arena->pools_usage[i] += ds_layout->desc_count[i];
    if (!rvk_alloc_ds(alloc_info, set)) {
        rvk_log_descriptor_pool_usage(*arena);
        return false;
    }
    return true;
}

const char *rvk_desc_type_to_str(Rvk_Descriptor_Type type)
{
    switch (type) {
    case RVK_DESCRIPTOR_TYPE_SAMPLER:                return "sampler";
    case RVK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "combined_image_sampler";
    case RVK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return "sampled_image";
    case RVK_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return "storage_image";
    case RVK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:   return "uniform_texel_buffer";
    case RVK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:   return "storage_texel_buffer";
    case RVK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return "uniform_buffer";
    case RVK_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return "storage_buffer";
    case RVK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "uniform_buffer_dynamic";
    case RVK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "storage_buffer_dynamic";
    case RVK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:       return "input_attachment";
    default:
        fprintf(stderr, "descriptor type unrecognized %d\n", type);
        return "unrecognized";
    }
}

void rvk_log_descriptor_pool_usage(Rvk_Descriptor_Pool_Arena arena)
{
    rvk_log(RVK_INFO, "Descriptor pool usage:");
    rvk_log(RVK_INFO, "only increases when you call: rvk_descriptor_pool_arena_alloc_set(...)");
    rvk_log(RVK_INFO, "only resets to zero when you call: rvk_descriptor_pool_arena_reset(...)");
    rvk_log(RVK_INFO, "keep in mind there is no in between");
    rvk_log(RVK_INFO, "---------------------");
    for (size_t i = 0; i < RVK_DESCRIPTOR_TYPE_COUNT; i++)
        rvk_log(RVK_INFO, "    type %s: %zu", rvk_desc_type_to_str(i), arena.pools_usage[i]);
    rvk_log(RVK_INFO, "---------------------");
}

void rvk_log_descriptor_layout_usage(Rvk_Descriptor_Set_Layout layout, const char *name)
{
    rvk_log(RVK_INFO, "descriptor set layout %p (%s) usage:", layout.handle, name ? name : "<no name>");
    for (size_t i = 0; i < RVK_DESCRIPTOR_TYPE_COUNT; i++)
        rvk_log(RVK_INFO, "    type %s: %zu", rvk_desc_type_to_str(i), layout.desc_count[i]);
}

void rvk_descriptor_pool_arena_destroy(Rvk_Descriptor_Pool_Arena arena)
{
    rvk_destroy_ds_pool(arena.pool);
}


void rvk_destroy_ds_pool(VkDescriptorPool pool)
{
    vkDestroyDescriptorPool(rvk_ctx.device, pool, NULL);
}

void rvk_destroy_descriptor_set_layout(VkDescriptorSetLayout layout)
{
    vkDestroyDescriptorSetLayout(rvk_ctx.device, layout, NULL);
}

bool rvk_alloc_ds(VkDescriptorSetAllocateInfo alloc, VkDescriptorSet *sets)
{
    VkResult res = vkAllocateDescriptorSets(rvk_ctx.device, &alloc, sets);
    if (res == VK_ERROR_OUT_OF_POOL_MEMORY) {
        rvk_log(RVK_ERROR, "out of pool memory");
        return false;
    } else if (!RVK_SUCCEEDED(res)) {
        rvk_log(RVK_ERROR, "failed to allocate descriptor sets");
        return false;
    }

    return true;
}

void rvk_update_ds(size_t count, VkWriteDescriptorSet *writes)
{
    vkUpdateDescriptorSets(rvk_ctx.device, (uint32_t)count, writes, 0, NULL);
}

void rvk_wait_idle()
{
    RAG_VK(vkDeviceWaitIdle(rvk_ctx.device));
}

#endif // RAG_VK_IMPLEMENTATION
