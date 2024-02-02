#include "cvr.h"
#include "ext_man.h"
#include "vk_cmd_man.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#include "common.h"

#define RAYMATH_IMPLEMENTATION
#include "ext/raylib-5.0/raymath.h"

#include <vulkan/vulkan.h>
#include "vk_ctx.h"
#include "vk_buffer.h"
#include "geometry.h"
#include <time.h>

#define Z_NEAR 0.01
#define Z_FAR 1000.0

Vk_Context ctx = {0};
Core_State core_state = {0};
static Vk_Cmd_Man cmd_man = {0};
extern Ext_Manager ext_manager;

#define MAX_MAT_STACK 1024 * 1024
Matrix mat_stack[MAX_MAT_STACK];
size_t mat_stack_p = 0;

static const size_t MAX_FRAMES_IN_FLIGHT = 2;
static uint32_t curr_frame = 0;
static uint32_t img_idx = 0;
static clock_t time_begin;

typedef struct {
    float16 model;
    float16 view;
    float16 proj;
} UBO;

bool cvr_init()
{
    bool result = true;

    time_begin = clock();
    init_ext_managner();

    cvr_chk(create_instance(), "failed to create instance");
#ifdef ENABLE_VALIDATION
    cvr_chk(setup_debug_msgr(), "failed to setup debug messenger");
#endif
    cvr_chk(create_surface(), "failed to create vulkan surface");
    cvr_chk(pick_phys_device(), "failed to find suitable GPU");
    cvr_chk(create_device(), "failed to create logical device");
    cvr_chk(create_swpchain(), "failed to create swapchain");
    cvr_chk(create_img_views(), "failed to create image views");
    cvr_chk(create_render_pass(), "failed to create render pass");
    cvr_chk(create_descriptor_set_layout(), "failed to create desciptorset layout");
    cvr_chk(create_frame_buffs(), "failed to create frame buffers");
    cmd_man.phys_device = ctx.phys_device;
    cmd_man.device = ctx.device;
    cmd_man.frames_in_flight = MAX_FRAMES_IN_FLIGHT;
    cvr_chk(cmd_man_init(&cmd_man), "failed to create vulkan command manager");
    cvr_chk(create_ubos(), "failed to create uniform buffer objects");
    cvr_chk(create_descriptor_pool(), "failed to create descriptor pool");
    cvr_chk(create_descriptor_sets(), "failed to create descriptor pool");

defer:
    return result;
}

bool cvr_destroy()
{
    vkDeviceWaitIdle(ctx.device);
    cmd_man_destroy(&cmd_man);
    cleanup_swpchain();
    for (size_t i = 0; i < ctx.ubos.count; i++)
        vk_buff_destroy(ctx.ubos.items[i]);
    nob_da_free(ctx.ubos);
    vkDestroyDescriptorPool(ctx.device, ctx.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, ctx.descriptor_set_layout, NULL);
    nob_da_free(ctx.descriptor_sets);
    for (size_t i = 0; i < SHAPE_COUNT; i++) {
        if (ctx.shapes[i].vtx_buff.handle)
            vk_buff_destroy(ctx.shapes[i].vtx_buff);
        if (ctx.shapes[i].idx_buff.handle)
            vk_buff_destroy(ctx.shapes[i].idx_buff);
    }
    vkDestroyPipeline(ctx.device, ctx.pipelines.shape, NULL);
    ctx.pipelines.shape = NULL;
    vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layout, NULL);
    vkDestroyRenderPass(ctx.device, ctx.render_pass, NULL);
    vkDestroyDevice(ctx.device, NULL);
#ifdef ENABLE_VALIDATION
    load_pfn(vkDestroyDebugUtilsMessengerEXT);
    if (vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debug_msgr, NULL);
#endif
    vkDestroySurfaceKHR(ctx.instance, ctx.surface, NULL);
    vkDestroyInstance(ctx.instance, NULL);
    destroy_ext_manager();
    return true;
}

bool create_instance()
{
    bool result = true;
#ifdef ENABLE_VALIDATION
    cvr_chk(chk_validation_support(), "validation requested, but not supported");
#endif

    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "C + Vulkan = Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_ci = {0};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;
    uint32_t glfw_ext_count = 0;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    for (size_t i = 0; i < glfw_ext_count; i++)
        nob_da_append(&ext_manager.inst_exts, glfw_exts[i]);
#ifdef ENABLE_VALIDATION
    instance_ci.enabledLayerCount = ext_manager.validation_layers.count;
    instance_ci.ppEnabledLayerNames = ext_manager.validation_layers.items;
    nob_da_append(&ext_manager.inst_exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
    populated_debug_msgr_ci(&debug_msgr_ci);
    instance_ci.pNext = &debug_msgr_ci;
#endif
    instance_ci.enabledExtensionCount = ext_manager.inst_exts.count;
    instance_ci.ppEnabledExtensionNames = ext_manager.inst_exts.items;

    cvr_chk(inst_exts_satisfied(), "unsatisfied instance extensions");
    vk_chk(vkCreateInstance(&instance_ci, NULL, &ctx.instance), "failed to create vulkan instance");

defer:
    return result;
}

bool create_device()
{
    QueueFamilyIndices indices = find_queue_fams(ctx.phys_device);

    int queue_fams[] = {indices.gfx_idx, indices.present_idx};
    U32_Set unique_fams = {0};
    populate_set(queue_fams, NOB_ARRAY_LEN(queue_fams), &unique_fams);

    vec(VkDeviceQueueCreateInfo) queue_cis = {0};
    float queuePriority = 1.0f;
    for (size_t i = 0; i < unique_fams.count; i++) {
        VkDeviceQueueCreateInfo queue_ci = {0};
        queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.queueFamilyIndex = unique_fams.items[i];
        queue_ci.queueCount = 1;
        queue_ci.pQueuePriorities = &queuePriority;
        nob_da_append(&queue_cis, queue_ci);
    }

    VkDeviceCreateInfo device_ci = {0};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    VkPhysicalDeviceFeatures features = {0};
    features.fillModeNonSolid = VK_TRUE;
    device_ci.pEnabledFeatures = &features;
    device_ci.pQueueCreateInfos = queue_cis.items;
    device_ci.queueCreateInfoCount = queue_cis.count;
    device_ci.enabledExtensionCount = ext_manager.device_exts.count;
    device_ci.ppEnabledExtensionNames = ext_manager.device_exts.items;
#ifdef ENABLE_VALIDATION
    device_ci.enabledLayerCount = ext_manager.validation_layers.count;
    device_ci.ppEnabledLayerNames = ext_manager.validation_layers.items;
#endif

    bool result = true;
    if (vk_ok(vkCreateDevice(ctx.phys_device, &device_ci, NULL, &ctx.device))) {
        vkGetDeviceQueue(ctx.device, indices.gfx_idx, 0, &ctx.gfx_queue);
        vkGetDeviceQueue(ctx.device, indices.present_idx, 0, &ctx.present_queue);
    } else {
        nob_return_defer(false);
    }

defer:
    nob_da_free(queue_cis);
    return result;
}

bool create_surface()
{
    return vk_ok(glfwCreateWindowSurface(ctx.instance, ctx.window, NULL, &ctx.surface));
}

bool create_swpchain()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.phys_device, ctx.surface, &capabilities);
    ctx.surface_fmt = choose_swpchain_fmt();
    uint32_t img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && img_count > capabilities.maxImageCount)
        img_count = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swpchain_ci = {0};
    swpchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swpchain_ci.surface = ctx.surface;
    swpchain_ci.minImageCount = img_count;
    swpchain_ci.imageFormat = ctx.surface_fmt.format;
    swpchain_ci.imageColorSpace = ctx.surface_fmt.colorSpace;
    swpchain_ci.imageExtent = ctx.extent = choose_swp_extent();
    swpchain_ci.imageArrayLayers = 1;
    swpchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    QueueFamilyIndices indices = find_queue_fams(ctx.phys_device);
    uint32_t queue_fams[] = {indices.gfx_idx, indices.present_idx};
    if (indices.gfx_idx != indices.present_idx) {
        swpchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swpchain_ci.queueFamilyIndexCount = 2;
        swpchain_ci.pQueueFamilyIndices = queue_fams;
    } else {
        swpchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    swpchain_ci.clipped = VK_TRUE;
    swpchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swpchain_ci.presentMode = choose_present_mode();
    swpchain_ci.preTransform = capabilities.currentTransform;

    if (vk_ok(vkCreateSwapchainKHR(ctx.device, &swpchain_ci, NULL, &ctx.swpchain.handle))) {
        uint32_t img_count = 0;
        vkGetSwapchainImagesKHR(ctx.device, ctx.swpchain.handle, &img_count, NULL);
        nob_da_resize(&ctx.swpchain.imgs, img_count);
        vkGetSwapchainImagesKHR(ctx.device, ctx.swpchain.handle, &img_count, ctx.swpchain.imgs.items);
        return true;
    } else {
        return false;
    }
}

bool create_img_views()
{
    nob_da_resize(&ctx.swpchain.img_views, ctx.swpchain.imgs.count);
    for (size_t i = 0; i < ctx.swpchain.img_views.count; i++)  {
        VkImageViewCreateInfo img_view_ci = {0};
        img_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        img_view_ci.image = ctx.swpchain.imgs.items[i];
        img_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        img_view_ci.format = ctx.surface_fmt.format;
        img_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_view_ci.subresourceRange.baseMipLevel = 0;
        img_view_ci.subresourceRange.levelCount = 1;
        img_view_ci.subresourceRange.baseArrayLayer = 0;
        img_view_ci.subresourceRange.layerCount = 1;
        if (!vk_ok(vkCreateImageView(ctx.device, &img_view_ci, NULL, &ctx.swpchain.img_views.items[i])))
            return false;
    }

    return true;
}

bool create_shape_pipeline()
{
    bool result = true;
    VkPipelineShaderStageCreateInfo vert_ci = {0};
    vert_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_ci.pName = "main";
    if (!create_shader_module("./shaders/shader.vert.spv", &vert_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo frag_ci = {0};
    frag_ci .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_ci .stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_ci.pName = "main";
    if (!create_shader_module("./shaders/shader.frag.spv", &frag_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo stages[] = {vert_ci, frag_ci};

    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {0};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    dynamic_state_ci.dynamicStateCount = NOB_ARRAY_LEN(dynamic_states);
    dynamic_state_ci.pDynamicStates = dynamic_states;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {0};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VtxAttrDescs vert_attrs = {0};
    get_attr_descs(&vert_attrs);
    VkVertexInputBindingDescription binding_desc = get_binding_desc();
    vertex_input_ci.vertexBindingDescriptionCount = 1;
    vertex_input_ci.pVertexBindingDescriptions = &binding_desc;
    vertex_input_ci.vertexAttributeDescriptionCount = vert_attrs.count;
    vertex_input_ci.pVertexAttributeDescriptions = vert_attrs.items;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {0};
    input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_ci.topology = core_state.topology;

    VkViewport viewport = {0};
    viewport.width = (float) ctx.extent.width;
    viewport.height = (float) ctx.extent.height;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {0};
    scissor.extent = ctx.extent;
    VkPipelineViewportStateCreateInfo viewport_state_ci = {0};
    viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.viewportCount = 1;
    viewport_state_ci.pViewports = &viewport;
    viewport_state_ci.scissorCount = 1;
    viewport_state_ci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {0};
    rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // rasterizer_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_ci.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer_ci.lineWidth = 1.0f;
    // rasterizer_ci.cullMode = VK_CULL_MODE_FRONT_BIT;
    // rasterizer_ci.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_ci.cullMode = VK_CULL_MODE_NONE;
    rasterizer_ci.lineWidth = VK_FRONT_FACE_CLOCKWISE;
    // rasterizer_ci.lineWidth = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling_ci = {0};
    multisampling_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend = {0};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                 VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT |
                                 VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo color_blend_ci = {0};
    color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_ci.attachmentCount = 1;
    color_blend_ci.pAttachments = &color_blend;

    VkPipelineLayoutCreateInfo pipeline_layout_ci = {0};
    pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_ci.pSetLayouts = &ctx.descriptor_set_layout;
    pipeline_layout_ci.setLayoutCount = 1;
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(float16),
    };
    pipeline_layout_ci.pPushConstantRanges = &pk_range;
    pipeline_layout_ci.pushConstantRangeCount = 1;
    VkResult vk_result = vkCreatePipelineLayout(
        ctx.device,
        &pipeline_layout_ci,
        NULL,
        &ctx.pipeline_layout
    );
    vk_chk(vk_result, "failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipeline_ci = {0};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = NOB_ARRAY_LEN(stages);
    pipeline_ci.pStages = stages;
    pipeline_ci.pVertexInputState = &vertex_input_ci;
    pipeline_ci.pInputAssemblyState = &input_assembly_ci;
    pipeline_ci.pViewportState = &viewport_state_ci;
    pipeline_ci.pRasterizationState = &rasterizer_ci;
    pipeline_ci.pMultisampleState = &multisampling_ci;
    pipeline_ci.pColorBlendState = &color_blend_ci;
    pipeline_ci.pDynamicState = &dynamic_state_ci;
    pipeline_ci.layout = ctx.pipeline_layout;
    pipeline_ci.renderPass = ctx.render_pass;
    pipeline_ci.subpass = 0;

    vk_result = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &ctx.pipelines.shape);
    vk_chk(vk_result, "failed to create pipeline");

defer:
    vkDestroyShaderModule(ctx.device, frag_ci.module, NULL);
    vkDestroyShaderModule(ctx.device, vert_ci.module, NULL);
    nob_da_free(vert_attrs);
    return result;
}

bool create_shader_module(const char *file_name, VkShaderModule *module)
{
    bool result = true;
    Nob_String_Builder sb = {};
    char *err_msg = nob_temp_sprintf("failed to read entire file %s", file_name);
    cvr_chk(nob_read_entire_file(file_name, &sb), err_msg);

    VkShaderModuleCreateInfo module_ci = {0};
    module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_ci.codeSize = sb.count;
    module_ci.pCode = (const uint32_t *)sb.items;
    err_msg = nob_temp_sprintf("failed to create shader module from %s", file_name);
    vk_chk(vkCreateShaderModule(ctx.device, &module_ci, NULL, module), err_msg);

defer:
    nob_sb_free(sb);
    return result;
}

bool create_render_pass()
{
    VkAttachmentDescription color_attach = {0};
    color_attach.format = ctx.surface_fmt.format;
    color_attach.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attach.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attach_ref = {0};
    color_attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription gfx_subpass = {0};
    gfx_subpass.colorAttachmentCount = 1;
    gfx_subpass.pColorAttachments = &color_attach_ref;

    VkRenderPassCreateInfo render_pass_ci = {0};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = 1;
    render_pass_ci.pAttachments = &color_attach;
    render_pass_ci.subpassCount = 1;
    render_pass_ci.pSubpasses = &gfx_subpass;
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    render_pass_ci.dependencyCount = 1;
    render_pass_ci.pDependencies = &dependency;

    return vk_ok(vkCreateRenderPass(ctx.device, &render_pass_ci, NULL, &ctx.render_pass));
}

bool create_frame_buffs()
{
    nob_da_resize(&ctx.swpchain.buffs, ctx.swpchain.img_views.count);
    for (size_t i = 0; i < ctx.swpchain.img_views.count; i++) {
        VkFramebufferCreateInfo frame_buff_ci = {0};
        frame_buff_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buff_ci.renderPass = ctx.render_pass;
        frame_buff_ci.attachmentCount = 1;
        frame_buff_ci.pAttachments = &ctx.swpchain.img_views.items[i];
        frame_buff_ci.width =  ctx.extent.width;
        frame_buff_ci.height = ctx.extent.height;
        frame_buff_ci.layers = 1;
        if (!vk_ok(vkCreateFramebuffer(ctx.device, &frame_buff_ci, NULL, &ctx.swpchain.buffs.items[i])))
            return false;
    }

    return true;
}

void cvr_begin_render_pass(Color color)
{
    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];

    VkRenderPassBeginInfo begin_rp = {0};
    begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_rp.renderPass = ctx.render_pass;
    begin_rp.framebuffer = ctx.swpchain.buffs.items[img_idx];
    begin_rp.renderArea.extent = ctx.extent;
    VkClearValue clear_color = {0};
    clear_color.color.float32[0] = color.r / 255.0f;
    clear_color.color.float32[1] = color.g / 255.0f;
    clear_color.color.float32[2] = color.b / 255.0f;
    clear_color.color.float32[3] = color.a / 255.0f;
    begin_rp.clearValueCount = 1;
    begin_rp.pClearValues = &clear_color;
    vkCmdBeginRenderPass(cmd_buffer, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
}

bool cvr_draw_shape(Shape_Type shape_type)
{
    bool result = true;

    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipelines.shape);
    VkViewport viewport = {0};
    viewport.width = (float)ctx.extent.width;
    viewport.height =(float)ctx.extent.height;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = ctx.extent;
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    Vk_Buffer vtx_buff = ctx.shapes[shape_type].vtx_buff;
    Vk_Buffer idx_buff = ctx.shapes[shape_type].idx_buff;

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vtx_buff.handle, offsets);
    vkCmdBindIndexBuffer(cmd_buffer, idx_buff.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(
        cmd_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ctx.pipeline_layout, 0, 1, &ctx.descriptor_sets.items[curr_frame], 0, NULL
    );

    // flatten matrix stack
    Matrix model = MatrixIdentity();
    for (size_t i = 0; i < mat_stack_p; i++)
        model = MatrixMultiply(mat_stack[i], model);

    Matrix viewProj = MatrixMultiply(core_state.view, core_state.proj);
    Matrix mvp = MatrixMultiply(model, viewProj);

    float16 mat = MatrixToFloatV(mvp);
    vkCmdPushConstants(
        cmd_buffer,
        ctx.pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(float16),
        &mat
    );

    vkCmdDrawIndexed(cmd_buffer, idx_buff.count, 1, 0, 0, 0);

    return result;
}

bool begin_draw()
{
    bool result = true;
    VkResult vk_result = vkWaitForFences(ctx.device, 1, &cmd_man.fences.items[curr_frame], VK_TRUE, UINT64_MAX);
    vk_chk(vk_result, "failed to wait for fences");

    vk_result = vkAcquireNextImageKHR(ctx.device,
        ctx.swpchain.handle,
        UINT64_MAX,
        cmd_man.img_avail_sems.items[curr_frame],
        VK_NULL_HANDLE,
        &img_idx
    );
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        cvr_chk(recreate_swpchain(), "failed to recreate swapchain");
    } else if (!vk_ok(vk_result) && vk_result != VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_ERROR, "failed to acquire swapchain image");
        nob_return_defer(false);
    } else if (vk_result == VK_SUBOPTIMAL_KHR) {
        nob_log(NOB_WARNING, "suboptimal swapchain image");
    }

    cvr_update_ubos();

    vk_chk(vkResetFences(ctx.device, 1, &cmd_man.fences.items[curr_frame]), "failed to reset fences");
    vk_chk(vkResetCommandBuffer(cmd_man.buffs.items[curr_frame], 0), "failed to reset cmd buffer");

    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_chk(vkBeginCommandBuffer(cmd_buffer, &beginInfo), "failed to begin command buffer");

defer:
    return result;
}

bool end_draw()
{
    bool result = true;

    VkCommandBuffer cmd_buffer = cmd_man.buffs.items[curr_frame];
    vkCmdEndRenderPass(cmd_buffer);
    vk_chk(vkEndCommandBuffer(cmd_buffer), "failed to record command buffer");

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &cmd_man.img_avail_sems.items[curr_frame];
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_man.buffs.items[curr_frame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &cmd_man.render_fin_sems.items[curr_frame];

    vk_chk(vkQueueSubmit(ctx.gfx_queue, 1, &submit, cmd_man.fences.items[curr_frame]), "failed to submit command");

    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &cmd_man.render_fin_sems.items[curr_frame];
    present.swapchainCount = 1;
    present.pSwapchains = &ctx.swpchain.handle;
    present.pImageIndices = &img_idx;

    VkResult vk_result = vkQueuePresentKHR(ctx.present_queue, &present);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR || ctx.swpchain.buff_resized) {
        ctx.swpchain.buff_resized = false;
        cvr_chk(recreate_swpchain(), "failed to recreate swapchain");
    } else if (!vk_ok(vk_result)) {
        nob_log(NOB_ERROR, "failed to present queue");
        nob_return_defer(false);
    }

    curr_frame = (curr_frame + 1) % MAX_FRAMES_IN_FLIGHT;

defer:
    return result;
}

bool recreate_swpchain()
{
    bool result = true;

    int width = 0, height = 0;
    glfwGetFramebufferSize(ctx.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(ctx.window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx.device);

    cleanup_swpchain();

    cvr_chk(create_swpchain(), "failed to recreate swapchain");
    cvr_chk(create_img_views(), "failed to recreate image views");
    cvr_chk(create_frame_buffs(), "failed to recreate frame buffers");

defer:
    return result;
}

bool create_shape_vtx_buffer(Shape *shape)
{
    bool result = true;
    Vk_Buffer stg_buff;
    shape->vtx_buff.device = stg_buff.device = ctx.device;
    shape->vtx_buff.size   = stg_buff.size   = shape->vert_size * shape->vert_count;
    shape->vtx_buff.count  = stg_buff.count  = shape->vert_count;
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    void* data;
    vk_chk(vkMapMemory(ctx.device, stg_buff.buff_mem, 0, stg_buff.size, 0, &data), "failed to map memory");
    memcpy(data, shape->verts, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.buff_mem);

    result = vk_buff_init(
        &shape->vtx_buff,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create vertex buffer");

    vk_buff_copy(&cmd_man, ctx.gfx_queue, stg_buff, shape->vtx_buff, 0);

defer:
    vk_buff_destroy(stg_buff);
    return result;
}

bool create_shape_idx_buffer(Shape *shape)
{
    bool result = true;
    Vk_Buffer stg_buff;
    shape->idx_buff.device = stg_buff.device = ctx.device;
    shape->idx_buff.size   = stg_buff.size   = shape->idx_size * shape->idx_count;
    shape->idx_buff.count  = stg_buff.count  = shape->idx_count;
    result = vk_buff_init(
        &stg_buff,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    void* data;
    vk_chk(vkMapMemory(ctx.device, stg_buff.buff_mem, 0, stg_buff.size, 0, &data), "failed to map memory");
    memcpy(data, shape->idxs, stg_buff.size);
    vkUnmapMemory(ctx.device, stg_buff.buff_mem);

    result = vk_buff_init(
        &shape->idx_buff,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create index buffer");

    vk_buff_copy(&cmd_man, ctx.gfx_queue, stg_buff, shape->idx_buff, 0);

defer:
    vk_buff_destroy(stg_buff);
    return result;

}

bool create_descriptor_set_layout()
{
    VkDescriptorSetLayoutBinding layout = {0};
    layout.binding = 0;
    layout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layout.descriptorCount = 1;
    layout.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layout_ci = {0};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = 1;
    layout_ci.pBindings = &layout;
    return vk_ok(vkCreateDescriptorSetLayout(ctx.device, &layout_ci, NULL, &ctx.descriptor_set_layout));
}

bool create_ubos()
{
    bool result = true;

    nob_da_resize(&ctx.ubos, MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        Vk_Buffer *buff = &ctx.ubos.items[i];
        buff->size = sizeof(UBO);
        buff->device = ctx.device;
        result = vk_buff_init(
            buff,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        cvr_chk(result, "failed to create uniform buffer");
        vkMapMemory(ctx.device, buff->buff_mem, 0, buff->size, 0, &buff->mapped);
    }

defer:
     return result;
}

void cvr_update_ubos()
{
    UBO ubo = {
        .model = MatrixToFloatV(MatrixIdentity()),
        .view  = MatrixToFloatV(core_state.view),
        .proj  = MatrixToFloatV(core_state.proj),
    };

    memcpy(ctx.ubos.items[curr_frame].mapped, &ubo, sizeof(ubo));
}

bool create_descriptor_pool()
{
    VkDescriptorPoolSize pool_size = {0};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo pool_ci = {0};
    pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.poolSizeCount = 1;
    pool_ci.pPoolSizes = &pool_size;
    pool_ci.maxSets = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    return vk_ok(vkCreateDescriptorPool(ctx.device, &pool_ci, NULL, &ctx.descriptor_pool));
}

bool create_descriptor_sets()
{
    bool result = true;

    vec(VkDescriptorSetLayout) layouts = {0};
    nob_da_resize(&layouts, MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < layouts.count; i++)
        layouts.items[i] = ctx.descriptor_set_layout;

    VkDescriptorSetAllocateInfo alloc = {0};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = ctx.descriptor_pool;
    alloc.descriptorSetCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;
    alloc.pSetLayouts = layouts.items;

    nob_da_resize(&ctx.descriptor_sets, MAX_FRAMES_IN_FLIGHT);
    VkResult vk_result = vkAllocateDescriptorSets(ctx.device, &alloc, ctx.descriptor_sets.items);
    vk_chk(vk_result, "failed to allocate descriptor sets");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buff_info = {0};
        buff_info.buffer = ctx.ubos.items[i].handle;
        buff_info.offset = 0;
        buff_info.range = sizeof(UBO);

        VkWriteDescriptorSet descriptor_write = {0};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = ctx.descriptor_sets.items[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;

        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buff_info;
        vkUpdateDescriptorSets(ctx.device, 1, &descriptor_write, 0, NULL);
    }

defer:
    nob_da_free(layouts);
    return result;
}

bool is_device_suitable(VkPhysicalDevice phys_device)
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(phys_device);
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(phys_device, &props);
    vkGetPhysicalDeviceFeatures(phys_device, &features);
    result &= (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
               props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
    result &= (features.geometryShader);
    cvr_chk(indices.has_gfx && indices.has_present, "requested indices not present");
    cvr_chk(device_exts_supported(phys_device), "device extensions not supported");
    cvr_chk(swpchain_adequate(phys_device), "swapchain was not adequate");

defer:
    return result;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data)
{
    unused(msg_type);
    unused(p_user_data);
    Nob_Log_Level log_lvl = translate_msg_severity(msg_severity);
    if (log_lvl < MIN_SEVERITY) return VK_FALSE;
    nob_log(log_lvl, "[Vulkan Validation] %s", p_callback_data->pMessage);
    return VK_FALSE;
}

void populated_debug_msgr_ci(VkDebugUtilsMessengerCreateInfoEXT *debug_msgr_ci)
{
    debug_msgr_ci->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_msgr_ci->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_msgr_ci->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_msgr_ci->pfnUserCallback = debug_callback;
}

Nob_Log_Level translate_msg_severity(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity)
{
    switch (msg_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return NOB_INFO;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return NOB_INFO;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return NOB_WARNING;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return NOB_ERROR;
    default: assert(0 && "this message severity is not handled");
    }
}

bool setup_debug_msgr()
{
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_ci = {0};
    populated_debug_msgr_ci(&debug_msgr_ci);
    load_pfn(vkCreateDebugUtilsMessengerEXT);
    if (vkCreateDebugUtilsMessengerEXT) {
        return vk_ok(vkCreateDebugUtilsMessengerEXT(ctx.instance, &debug_msgr_ci, NULL, &ctx.debug_msgr));
    } else {
        nob_log(NOB_ERROR, "failed to load function pointer for vkCreateDebugUtilesMessenger");
        return false;
    }
}

QueueFamilyIndices find_queue_fams(VkPhysicalDevice phys_device)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fams[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_fam_count, queue_fams);
    QueueFamilyIndices indices = {0};
    for (size_t i = 0; i < queue_fam_count; i++) {
        if (queue_fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.gfx_idx = i;
            indices.has_gfx = true;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, ctx.surface, &present_support);
        if (present_support) {
            indices.present_idx = i;
            indices.has_present = true;
        }

        if (indices.has_gfx && indices.has_present)
            return indices;
    }
    return indices;
}

bool pick_phys_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, NULL);
    VkPhysicalDevice phys_devices[device_count];
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, phys_devices);
    for (size_t i = 0; i < device_count; i++) {
        if (is_device_suitable(phys_devices[i])) {
            ctx.phys_device = phys_devices[i];
            return true;
        }
    }

    return false;
}

void populate_set(int arr[], size_t arr_size, U32_Set *set)
{
    for (size_t i = 0; i < arr_size; i++) {
        if (arr[i] == -1)
            continue;

        nob_da_append(set, arr[i]);

        for (size_t j = i + 1; j < arr_size; j++)
            if (arr[i] == arr[j])
                arr[j] = -1;
    }
}

bool swpchain_adequate(VkPhysicalDevice phys_device)
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, ctx.surface, &surface_fmt_count, NULL);
    if (!surface_fmt_count) return false;
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, ctx.surface, &present_mode_count, NULL);
    if (!present_mode_count) return false;

    return true;
}

VkSurfaceFormatKHR choose_swpchain_fmt()
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.phys_device, ctx.surface, &surface_fmt_count, NULL);
    VkSurfaceFormatKHR fmts[surface_fmt_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.phys_device, ctx.surface, &surface_fmt_count, fmts);
    for (size_t i = 0; i < surface_fmt_count; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmts[i];
    }

    assert(surface_fmt_count && "surface format count was zero");
    return fmts[0];
}

VkPresentModeKHR choose_present_mode()
{
    uint32_t present_mode_count = 0; 
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.phys_device, ctx.surface, &present_mode_count, NULL);
    VkPresentModeKHR present_modes[present_mode_count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.phys_device, ctx.surface, &present_mode_count, present_modes);
    for (size_t i = 0; i < present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_modes[i];
    }

    assert(present_mode_count && "present mode count was zero");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swp_extent()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.phys_device, ctx.surface, &capabilities);
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(ctx.window, &width, &height);

        VkExtent2D extent = {
            .width = width,
            .height = height
        };

        extent.width = clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

void cleanup_swpchain()
{
    for (size_t i = 0; i < ctx.swpchain.buffs.count; i++)
        vkDestroyFramebuffer(ctx.device, ctx.swpchain.buffs.items[i], NULL);
    for (size_t i = 0; i < ctx.swpchain.img_views.count; i++)
        vkDestroyImageView(ctx.device, ctx.swpchain.img_views.items[i], NULL);
    vkDestroySwapchainKHR(ctx.device, ctx.swpchain.handle, NULL);

    nob_da_reset(ctx.swpchain.buffs);
    nob_da_reset(ctx.swpchain.img_views);
    nob_da_reset(ctx.swpchain.imgs);
}

bool find_mem_type_idx(uint32_t type, VkMemoryPropertyFlags properties, uint32_t *idx)
{
    VkPhysicalDeviceMemoryProperties mem_properites = {0};
    vkGetPhysicalDeviceMemoryProperties(ctx.phys_device, &mem_properites);
    for (uint32_t i = 0; i < mem_properites.memoryTypeCount; i++) {
        if (type & (1 << i) && (mem_properites.memoryTypes[i].propertyFlags & properties) == properties) {
            *idx = i;
            return true;
        }
    }

    return false;
}

void frame_buff_resized(GLFWwindow* window, int width, int height)
{
    unused(window);
    unused(width);
    unused(height);
    ctx.swpchain.buff_resized = true;
}

void close_window()
{
    cvr_destroy();
    glfwDestroyWindow(ctx.window);
    glfwTerminate();
}

bool alloc_shape_res(Shape_Type shape_type)
{
    bool result = true;

    if (is_shape_res_alloc(shape_type)) {
        nob_log(NOB_WARNING, "Shape resources for shape %d have already been allocated", shape_type);
        nob_return_defer(false);
    }

    Shape *shape = &ctx.shapes[shape_type];
    switch (shape_type) {
#define X(CONST_NAME, array_name)                        \
    case SHAPE_ ## CONST_NAME:                           \
    shape->vert_count = CONST_NAME ## _VERTS;            \
    shape->vert_size = sizeof(array_name ## _verts[0]);  \
    shape->verts = array_name ## _verts;                 \
    shape->idx_count = CONST_NAME ## _IDXS;              \
    shape->idx_size = sizeof(array_name ## _indices[0]); \
    shape->idxs = array_name ## _indices;                \
    break;
    SHAPE_LIST
#undef X
    default:
        nob_log(NOB_ERROR, "unrecognized shape %d", shape_type);
        nob_return_defer(false);
    }

    if ((!create_shape_vtx_buffer(shape)) || (!create_shape_idx_buffer(shape))) {
        nob_log(NOB_ERROR, "failed to allocate resources for shape %d", shape_type);
        nob_return_defer(false);
    }

defer:
    return result;
}

bool is_shape_res_alloc(Shape_Type shape_type)
{
    assert((shape_type >= 0 && shape_type < SHAPE_COUNT) && "invalid shape");
    return (ctx.shapes[shape_type].vtx_buff.handle || ctx.shapes[shape_type].idx_buff.handle);
}

bool cvr_push_matrix()
{
    if (mat_stack_p < MAX_MAT_STACK)
        mat_stack[mat_stack_p++] = MatrixIdentity();
    else
        return false;
    return true;
}

void cvr_set_proj(Camera camera)
{
    Matrix proj = {0};
    double aspect = ctx.extent.width / (double) ctx.extent.height;
    double top = camera.fovy / 2.0;
    double right = top * aspect;
    switch (camera.projection) {
    case PERSPECTIVE:
        proj  = MatrixPerspective(camera.fovy * DEG2RAD, aspect, Z_NEAR, Z_FAR);
        break;
    case ORTHOGRAPHIC:
        proj  = MatrixOrtho(-right, right, -top, top, -Z_FAR, Z_FAR);
        break;
    default:
        assert(0 && "unrecognized camera mode");
        break;
    }

    /* Vulkan */
    proj.m5 *= -1.0f;

    core_state.proj = proj;

}

void cvr_set_view(Camera camera)
{
    core_state.view = MatrixLookAt(camera.position, camera.target, camera.up);
}

bool cvr_pop_matrix()
{
    if (mat_stack_p > 0)
        mat_stack_p--;
    else
        return false;
    return true;
}

bool cvr_translate(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixTranslate(x, y, z), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}

bool cvr_rotate(Vector3 axis, float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotate(axis, angle), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}

bool cvr_rotate_x(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateX(angle), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}

bool cvr_rotate_y(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateY(angle), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}

bool cvr_rotate_z(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateZ(angle), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}

bool cvr_rotate_xyz(Vector3 angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateXYZ(angle), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}

bool cvr_rotate_zyx(Vector3 angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateZYX(angle), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}

bool cvr_scale(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixScale(x, y, z), mat_stack[mat_stack_p - 1]);
    else
      return false;
    return true;
}
