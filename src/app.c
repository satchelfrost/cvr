#include "app.h"
#include "app_utils.h"
#include "ext_man.h"
#include "vertex.h"

static const Vertex vertices[] = {
    {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}
};

/* Define the main application */
App app = {0};
static const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
static const size_t MAX_FRAMES_IN_FLIGHT = 2;
static uint32_t curr_frame = 0;
extern ExtManager ext_manager; // ext_man.c

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

bool create_cmd_pool()
{
    bool result = true;
    QueueFamilyIndices indices = find_queue_fams(app.phys_device);
    cvr_chk(indices.has_gfx, "failed to create command pool, no graphics queue");
    VkCommandPoolCreateInfo cmd_pool_ci = {0};
    cmd_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_ci.queueFamilyIndex = indices.gfx_idx;
    vk_chk(vkCreateCommandPool(app.device, &cmd_pool_ci, NULL, &app.cmd.pool), "failed to create command pool");

defer:
    return result;
}

bool create_cmd_buff()
{
    VkCommandBufferAllocateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.commandPool = app.cmd.pool;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    nob_da_resize(&app.cmd.buffs, MAX_FRAMES_IN_FLIGHT);
    ci.commandBufferCount = app.cmd.buffs.count;
    return vk_ok(vkAllocateCommandBuffers(app.device, &ci, app.cmd.buffs.items));
}

bool create_syncs()
{
    bool result = true;
    VkSemaphoreCreateInfo sem_ci = {0};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci = {0};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    nob_da_resize(&app.cmd.img_avail_sems, MAX_FRAMES_IN_FLIGHT);
    nob_da_resize(&app.cmd.render_finished_sems, MAX_FRAMES_IN_FLIGHT);
    nob_da_resize(&app.cmd.fences, MAX_FRAMES_IN_FLIGHT);
    VkResult vk_result;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk_result = vkCreateSemaphore(app.device, &sem_ci, NULL, &app.cmd.img_avail_sems.items[i]);
        vk_chk(vk_result, "failed to create semaphore");
        vk_result = vkCreateSemaphore(app.device, &sem_ci, NULL, &app.cmd.render_finished_sems.items[i]);
        vk_chk(vk_result, "failed to create semaphore");
        vk_result = vkCreateFence(app.device, &fence_ci, NULL, &app.cmd.fences.items[i]);
        vk_chk(vk_result, "failed to create fence");
    }

defer:
    return result;
}

bool draw()
{
    bool result = true;

    VkResult vk_result = vkWaitForFences(app.device, 1, &app.cmd.fences.items[curr_frame], VK_TRUE, UINT64_MAX);
    vk_chk(vk_result, "failed to wait for fences");

    uint32_t img_idx = 0;
    vk_result = vkAcquireNextImageKHR(app.device,
        app.swpchain.handle,
        UINT64_MAX,
        app.cmd.img_avail_sems.items[curr_frame],
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

    vk_chk(vkResetFences(app.device, 1, &app.cmd.fences.items[curr_frame]), "failed to reset fences");
    vk_chk(vkResetCommandBuffer(app.cmd.buffs.items[curr_frame], 0), "failed to reset cmd buffer");
    rec_cmds(img_idx, app.cmd.buffs.items[curr_frame]);

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &app.cmd.img_avail_sems.items[curr_frame];
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &app.cmd.buffs.items[curr_frame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &app.cmd.render_finished_sems.items[curr_frame];

    vk_chk(vkQueueSubmit(app.gfx_queue, 1, &submit, app.cmd.fences.items[curr_frame]), "failed to submit command");

    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &app.cmd.render_finished_sems.items[curr_frame];
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
    vkCmdDraw(cmd_buffer, NOB_ARRAY_LEN(vertices), 1, 0, 0);

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
    VkDeviceSize buff_size = sizeof(vertices);
    result = create_cvr_buffer(
        &app.vtx,
        buff_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    cvr_chk(result, "failed to create vertex buffer");

    vk_chk(vkBindBufferMemory(app.device, app.vtx.buff, app.vtx.buff_mem, 0), "failed to bind buffer memory");
    void* data;
    vk_chk(vkMapMemory(app.device, app.vtx.buff_mem, 0, buff_size, 0, &data), "failed to map memory");
    memcpy(data, vertices, (size_t) buff_size);
    vkUnmapMemory(app.device, app.vtx.buff_mem);

defer:
    return result;
}

void destroy_buffer(CVR_Buffer buffer)
{
    vkDestroyBuffer(app.device, buffer.buff, NULL);
    vkFreeMemory(app.device, buffer.buff_mem, NULL);
}

void destroy_cmd(CVR_Cmd cmd)
{
    for (size_t i = 0; i < cmd.img_avail_sems.count; i++)
        vkDestroySemaphore(app.device, app.cmd.img_avail_sems.items[i], NULL);
    for (size_t i = 0; i < cmd.render_finished_sems.count; i++)
        vkDestroySemaphore(app.device, cmd.render_finished_sems.items[i], NULL);
    for (size_t i = 0; i < cmd.fences.count; i++)
        vkDestroyFence(app.device, cmd.fences.items[i], NULL);
    vkDestroyCommandPool(app.device, cmd.pool, NULL);
}

bool create_cvr_buffer(
    CVR_Buffer *buffer,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties)
{
    bool result = true;

    VkBufferCreateInfo buffer_ci = {0};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_chk(vkCreateBuffer(app.device, &buffer_ci, NULL, &buffer->buff), "failed to create buffer");

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(app.device, app.vtx.buff, &mem_reqs);

    VkMemoryAllocateInfo alloc_ci = {0};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    result = find_mem_type_idx(mem_reqs.memoryTypeBits, properties, &alloc_ci.memoryTypeIndex);
    cvr_chk(result, "Memory not suitable based on memory requirements");
    VkResult vk_result = vkAllocateMemory(app.device, &alloc_ci, NULL, &app.vtx.buff_mem);
    vk_chk(vk_result, "failed to allocate buffer memory!");

defer:
    return result;
}