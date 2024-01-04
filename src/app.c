#include "app.h"
#include "app_utils.h"
#include "ext_man.h"
#include "vertex.h"
#include "cvr_cmd.h"

extern ExtManager ext_manager; // ext_man.c
extern CVR_Cmd cmd;            // cvr_cmd.c
App app = {0};

static const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
static const size_t MAX_FRAMES_IN_FLIGHT = 2;
static uint32_t curr_frame = 0;
static const Vertex vertices[] = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
};
static const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0
};

typedef struct {
    Matrix model;
    Matrix view;
    Matrix proj;
} UBO;

bool app_ctor()
{
    bool result = true;
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
    // cvr_chk(create_descriptor_set_layout(), "failed to create desciptorset layout");
    cvr_chk(create_gfx_pipeline(), "failed to create graphics pipeline");
    cvr_chk(create_frame_buffs(), "failed to create frame buffers");
    cvr_chk(cmd_ctor(app.device, app.phys_device, MAX_FRAMES_IN_FLIGHT), "failed to create command objects");
    cvr_chk(create_vtx_buffer(), "failed to create vertex buffer");
    cvr_chk(create_idx_buffer(), "failed to create index buffer");
    // cvr_chk(create_ubos(), "failed to create uniform buffer objects");
    // cvr_chk(create_descriptor_pool(), "failed to create descriptor pool");
    // cvr_chk(create_descriptor_sets(), "failed to create descriptor pool");

defer:
    return result;
}

bool app_dtor()
{
    cleanup_swpchain();
    // for (size_t i = 0; i < app.ubos.count; i++)
    //     buffer_dtor(app.ubos.items[i]);
    // vkDestroyDescriptorPool(app.device, app.descriptor_pool, NULL);
    // vkDestroyDescriptorSetLayout(app.device, app.descriptor_set_layout, NULL);
    buffer_dtor(app.vtx);
    buffer_dtor(app.idx);
    cmd_dtor(app.device);
    vkDestroyPipeline(app.device, app.pipeline, NULL);
    vkDestroyPipelineLayout(app.device, app.pipeline_layout, NULL);
    vkDestroyRenderPass(app.device, app.render_pass, NULL);
    vkDestroyDevice(app.device, NULL);
#ifdef ENABLE_VALIDATION
    load_pfn(vkDestroyDebugUtilsMessengerEXT);
    if (vkDestroyDebugUtilsMessengerEXT)
        vkDestroyDebugUtilsMessengerEXT(app.instance, app.debug_msgr, NULL);
#endif
    vkDestroySurfaceKHR(app.instance, app.surface, NULL);
    vkDestroyInstance(app.instance, NULL);
    glfwDestroyWindow(app.window);
    glfwTerminate();
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
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
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
    vk_chk(vkCreateInstance(&instance_ci, NULL, &app.instance), "failed to create vulkan instance");

defer:
    return result;
}

bool create_device()
{
    QueueFamilyIndices indices = find_queue_fams(app.phys_device);

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
    if (vk_ok(vkCreateDevice(app.phys_device, &device_ci, NULL, &app.device))) {
        vkGetDeviceQueue(app.device, indices.gfx_idx, 0, &app.gfx_queue);
        vkGetDeviceQueue(app.device, indices.present_idx, 0, &app.present_queue);
    } else {
        nob_return_defer(false);
    }

defer:
    nob_da_free(queue_cis);
    return result;
}

bool create_surface()
{
    return vk_ok(glfwCreateWindowSurface(app.instance, app.window, NULL, &app.surface));
}

bool create_swpchain()
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app.phys_device, app.surface, &capabilities);
    app.surface_fmt = choose_swpchain_fmt();
    uint32_t img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && img_count > capabilities.maxImageCount)
        img_count = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swpchain_ci = {0};
    swpchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swpchain_ci.surface = app.surface;
    swpchain_ci.minImageCount = img_count;
    swpchain_ci.imageFormat = app.surface_fmt.format;
    swpchain_ci.imageColorSpace = app.surface_fmt.colorSpace;
    swpchain_ci.imageExtent = app.extent = choose_swp_extent();
    swpchain_ci.imageArrayLayers = 1;
    swpchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    QueueFamilyIndices indices = find_queue_fams(app.phys_device);
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

    if (vk_ok(vkCreateSwapchainKHR(app.device, &swpchain_ci, NULL, &app.swpchain.handle))) {
        uint32_t img_count = 0;
        vkGetSwapchainImagesKHR(app.device, app.swpchain.handle, &img_count, NULL);
        nob_da_resize(&app.swpchain.imgs, img_count);
        vkGetSwapchainImagesKHR(app.device, app.swpchain.handle, &img_count, app.swpchain.imgs.items);
        return true;
    } else {
        return false;
    }
}

bool create_img_views()
{
    nob_da_resize(&app.swpchain.img_views, app.swpchain.imgs.count);
    for (size_t i = 0; i < app.swpchain.img_views.count; i++)  {
        VkImageViewCreateInfo img_view_ci = {0};
        img_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        img_view_ci.image = app.swpchain.imgs.items[i];
        img_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        img_view_ci.format = app.surface_fmt.format;
        img_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        img_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_view_ci.subresourceRange.baseMipLevel = 0;
        img_view_ci.subresourceRange.levelCount = 1;
        img_view_ci.subresourceRange.baseArrayLayer = 0;
        img_view_ci.subresourceRange.layerCount = 1;
        if (!vk_ok(vkCreateImageView(app.device, &img_view_ci, NULL, &app.swpchain.img_views.items[i])))
            return false;
    }

    return true;
}

bool create_gfx_pipeline()
{
    bool result = true;
    VkPipelineShaderStageCreateInfo vert_ci = {0};
    vert_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_ci.pName = "main";
    if (!create_shader_module("./build/shaders/shader.vert.spv", &vert_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo frag_ci = {0};
    frag_ci .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_ci .stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_ci.pName = "main";
    if (!create_shader_module("./build/shaders/shader.frag.spv", &frag_ci.module))
        nob_return_defer(false);

    VkPipelineShaderStageCreateInfo stages[] = {vert_ci, frag_ci};

    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {0};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
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
    input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0};
    viewport.width = (float) app.extent.width;
    viewport.height = (float) app.extent.height;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {0};
    scissor.extent = app.extent;
    VkPipelineViewportStateCreateInfo viewport_state_ci = {0};
    viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.viewportCount = 1;
    viewport_state_ci.pViewports = &viewport;
    viewport_state_ci.scissorCount = 1;
    viewport_state_ci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {0};
    rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_ci.lineWidth = 1.0f;
    rasterizer_ci.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

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
    // pipeline_layout_ci.pSetLayouts = &app.descriptor_set_layout;
    // pipeline_layout_ci.setLayoutCount = 1;
    VkResult vk_result = vkCreatePipelineLayout(
        app.device,
        &pipeline_layout_ci,
        NULL,
        &app.pipeline_layout
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
    pipeline_ci.layout = app.pipeline_layout;
    pipeline_ci.renderPass = app.render_pass;
    pipeline_ci.subpass = 0;

    vk_result = vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &app.pipeline);
    vk_chk(vk_result, "failed to create pipeline");

defer:
    vkDestroyShaderModule(app.device, frag_ci.module, NULL);
    vkDestroyShaderModule(app.device, vert_ci.module, NULL);
    nob_da_free(vert_attrs);
    return result;
}

bool create_shader_module(const char *shader, VkShaderModule *module)
{
    bool result = true;
    Nob_String_Builder sb = {};
    char *err_msg = nob_temp_sprintf("failed to read entire file %s", shader);
    cvr_chk(nob_read_entire_file(shader, &sb), err_msg);

    VkShaderModuleCreateInfo module_ci = {0};
    module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_ci.codeSize = sb.count;
    module_ci.pCode = (const uint32_t *)sb.items;
    err_msg = nob_temp_sprintf("failed to create shader module from %s", shader);
    vk_chk(vkCreateShaderModule(app.device, &module_ci, NULL, module), err_msg);

defer:
    nob_sb_free(sb);
    return result;
}

bool create_render_pass()
{
    VkAttachmentDescription color_attach = {0};
    color_attach.format = app.surface_fmt.format;
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

    return vk_ok(vkCreateRenderPass(app.device, &render_pass_ci, NULL, &app.render_pass));
}

bool create_frame_buffs()
{
    nob_da_resize(&app.swpchain.buffs, app.swpchain.img_views.count);
    for (size_t i = 0; i < app.swpchain.img_views.count; i++) {
        VkFramebufferCreateInfo frame_buff_ci = {0};
        frame_buff_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buff_ci.renderPass = app.render_pass;
        frame_buff_ci.attachmentCount = 1;
        frame_buff_ci.pAttachments = &app.swpchain.img_views.items[i];
        frame_buff_ci.width =  app.extent.width;
        frame_buff_ci.height = app.extent.height;
        frame_buff_ci.layers = 1;
        if (!vk_ok(vkCreateFramebuffer(app.device, &frame_buff_ci, NULL, &app.swpchain.buffs.items[i])))
            return false;
    }

    return true;
}

bool draw()
{
    bool result = true;

    VkResult vk_result = vkWaitForFences(app.device, 1, &cmd.fences.items[curr_frame], VK_TRUE, UINT64_MAX);
    vk_chk(vk_result, "failed to wait for fences");

    uint32_t img_idx = 0;
    vk_result = vkAcquireNextImageKHR(app.device,
        app.swpchain.handle,
        UINT64_MAX,
        cmd.img_avail_sems.items[curr_frame],
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

    // update_ubos(curr_frame);

    vk_chk(vkResetFences(app.device, 1, &cmd.fences.items[curr_frame]), "failed to reset fences");
    vk_chk(vkResetCommandBuffer(cmd.buffs.items[curr_frame], 0), "failed to reset cmd buffer");
    rec_cmds(img_idx, cmd.buffs.items[curr_frame]);

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &cmd.img_avail_sems.items[curr_frame];
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd.buffs.items[curr_frame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &cmd.render_finished_sems.items[curr_frame];

    vk_chk(vkQueueSubmit(app.gfx_queue, 1, &submit, cmd.fences.items[curr_frame]), "failed to submit command");

    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &cmd.render_finished_sems.items[curr_frame];
    present.swapchainCount = 1;
    present.pSwapchains = &app.swpchain.handle;
    present.pImageIndices = &img_idx;

    vk_result = vkQueuePresentKHR(app.present_queue, &present);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR || app.swpchain.buff_resized) {
        app.swpchain.buff_resized = false;
        cvr_chk(recreate_swpchain(), "failed to recreate swapchain");
    } else if (!vk_ok(vk_result)) {
        nob_log(NOB_ERROR, "failed to present queue");
        nob_return_defer(false);
    }

    curr_frame = (curr_frame + 1) % MAX_FRAMES_IN_FLIGHT;

defer:
    return result;
}

bool rec_cmds(uint32_t img_idx, VkCommandBuffer cmd_buffer)
{
    bool result = true;
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_chk(vkBeginCommandBuffer(cmd_buffer, &beginInfo), "failed to begin command buffer");

    VkRenderPassBeginInfo begin_rp = {0};
    begin_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_rp.renderPass = app.render_pass;
    begin_rp.framebuffer = app.swpchain.buffs.items[img_idx];
    begin_rp.renderArea.extent = app.extent;
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    begin_rp.clearValueCount = 1;
    begin_rp.pClearValues = &clear_color;
    vkCmdBeginRenderPass(cmd_buffer, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.pipeline);
    VkViewport viewport = {0};
    viewport.width = (float)app.extent.width;
    viewport.height =(float)app.extent.height;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor = {0};
    scissor.extent = app.extent;
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &app.vtx.buff, offsets);
    vkCmdBindIndexBuffer(cmd_buffer, app.idx.buff, 0, VK_INDEX_TYPE_UINT16);
    // vkCmdBindDescriptorSets(
    //     cmd_buffer,
    //     VK_PIPELINE_BIND_POINT_GRAPHICS,
    //     app.pipeline_layout, 0, 1, &app.descriptor_sets.items[curr_frame], 0, NULL
    // );
    vkCmdDrawIndexed(cmd_buffer, app.idx.count, 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd_buffer);
    vk_chk(vkEndCommandBuffer(cmd_buffer), "failed to record command buffer");

defer:
    return result;
}

bool recreate_swpchain()
{
    bool result = true;

    int width = 0, height = 0;
    glfwGetFramebufferSize(app.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(app.window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(app.device);

    cleanup_swpchain();

    cvr_chk(create_swpchain(), "failed to recreate swapchain");
    cvr_chk(create_img_views(), "failed to recreate image views");
    cvr_chk(create_frame_buffs(), "failed to recreate frame buffers");

defer:
    return result;
}

bool create_vtx_buffer()
{
    bool result = true;
    CVR_Buffer stg;
    app.vtx.device = stg.device = app.device;
    app.vtx.size   = stg.size   = sizeof(vertices);
    app.vtx.count  = stg.count  = NOB_ARRAY_LEN(vertices);
    result = buffer_ctor(
        &stg,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    void* data;
    vk_chk(vkMapMemory(app.device, stg.buff_mem, 0, stg.size, 0, &data), "failed to map memory");
    memcpy(data, vertices, stg.size);
    vkUnmapMemory(app.device, stg.buff_mem);

    result = buffer_ctor(
        &app.vtx,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create vertex buffer");

    copy_buff(app.gfx_queue, stg, app.vtx, 0);

defer:
    buffer_dtor(stg);
    return result;
}

bool create_idx_buffer()
{
    bool result = true;
    CVR_Buffer stg;
    app.idx.device = stg.device = app.device;
    app.idx.size   = stg.size   = sizeof(indices);
    app.idx.count  = stg.count  = NOB_ARRAY_LEN(indices);
    result = buffer_ctor(
        &stg,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create staging buffer");

    void* data;
    vk_chk(vkMapMemory(app.device, stg.buff_mem, 0, stg.size, 0, &data), "failed to map memory");
    memcpy(data, indices, stg.size);
    vkUnmapMemory(app.device, stg.buff_mem);

    result = buffer_ctor(
        &app.idx,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    cvr_chk(result, "failed to create index buffer");

    copy_buff(app.gfx_queue, stg, app.idx, 0);

defer:
    buffer_dtor(stg);
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
    return vk_ok(vkCreateDescriptorSetLayout(app.device, &layout_ci, NULL, &app.descriptor_set_layout));
}

bool create_ubos()
{
    bool result = true;

    nob_da_resize(&app.ubos, MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        CVR_Buffer *buff = &app.ubos.items[i];
        buff->size = sizeof(UBO);
        buff->device = app.device;
        result = buffer_ctor(
            buff,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        cvr_chk(result, "failed to create uniform buffer");
        vkMapMemory(app.device, buff->buff_mem, 0, buff->size, 0, &buff->mapped);
    }

defer:
     return result;
}

void update_ubos(uint32_t curr_image)
{
     UBO ubo = {0};
     ubo.model = MatrixIdentity();
     ubo.view  = MatrixLookAt((Vector3) {2.0f, 2.0f, 2.0f}, Vector3Zero(), (Vector3) {0.0f, 0.0f, 1.0f});
     ubo.proj  = MatrixPerspective(45.0f * DEG2RAD, app.extent.width / (float) app.extent.height, 0.1f, 10.0f);

     memcpy(app.ubos.items[curr_image].mapped, &ubo, sizeof(ubo));
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

    return vk_ok(vkCreateDescriptorPool(app.device, &pool_ci, NULL, &app.descriptor_pool));
}

bool create_descriptor_sets()
{
    bool result = true;

    vec(VkDescriptorSetLayout) layouts = {0};
    nob_da_resize(&layouts, MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < layouts.count; i++)
        layouts.items[i] = app.descriptor_set_layout;

    VkDescriptorSetAllocateInfo alloc = {0};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = app.descriptor_pool;
    alloc.descriptorSetCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;
    alloc.pSetLayouts = layouts.items;

    nob_da_resize(&app.descriptor_sets, MAX_FRAMES_IN_FLIGHT);
    VkResult vk_result = vkAllocateDescriptorSets(app.device, &alloc, app.descriptor_sets.items);
    vk_chk(vk_result, "failed to allocate descriptor sets");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buff_info = {0};
        buff_info.buffer = app.ubos.items[i].buff;
        buff_info.offset = 0;
        buff_info.range = sizeof(UBO);

        VkWriteDescriptorSet descriptor_write = {0};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = app.descriptor_sets.items[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;

        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buff_info;
        vkUpdateDescriptorSets(app.device, 1, &descriptor_write, 0, NULL);
    }

defer:
    nob_da_free(layouts);
    return result;
}