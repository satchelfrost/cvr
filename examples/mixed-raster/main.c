#include "cvr.h"

#define POINT_COUNT 100000
#define WORKGROUP_SZ 1024
#define IMG_WORKGROUP_SZ 16
#define NUM_BATCHES 8
#define DEFAULT_WINDOW_WIDTH 1600
#define DEFAULT_WINDOW_HEIGHT 900

typedef unsigned int uint;
typedef struct {
    float x, y, z;
    uint color;
} Point_Vert;

typedef struct {
    Point_Vert *items;
    size_t count;
    size_t capacity;
    Vk_Buffer buff;
} Point_Cloud;

typedef struct {
    Vk_Buffer buff;
    void *data;
} Frame_Buffer;

typedef struct {
    float16 mvp;
    int width;
    int height;
} UBO_Data;

typedef struct {
    Vk_Buffer buff;
    UBO_Data data;
} Point_Cloud_UBO;

typedef struct {
    VkFramebuffer frame_buff;
    Vk_Texture depth;
    Vk_Texture color;
    VkRenderPass rp;
    VkImageView img_views[2];
} Prepass;

typedef struct {
    VkPipeline pl;
    VkPipelineLayout pl_layout;
    VkDescriptorSetLayout ds_layout;
    VkDescriptorSet ds;
} Pipeline;

/* pipelines */
Pipeline prepass_gfx  = {0}; // 1) pre pass for 3d models / fixed function pipeline
Pipeline comp_mix     = {0}; // 2) combine the depth + color from prepass into one framebuff
Pipeline comp_render  = {0}; // 3) compute shader rasterization
Pipeline comp_resolve = {0}; // 4) resolve colors from compute shader raster
Pipeline sst_gfx      = {0}; // 5) draw resolved colors to a screen space triangle
VkDescriptorPool pool;
Prepass prepass = {0};

Point_Cloud gen_point_cloud(size_t num_points)
{
    Point_Cloud pc = {0};

    float phi = M_PI * (3.0 - sqrt(5.0));
    for (size_t i = 0; i < num_points; i++) {
        float y = 1.0 - (i / (float)(num_points - 1) * 2.0);
        float r = sqrt(1.0 - y * y);
        float theta = phi * i;
        float x = cos(theta) * r;
        float z = sin(theta) * r;
        Color color = color_from_HSV(r * 360.0f, 1.0f, 1.0f);
        uint uint_color = (uint)color.a << 24 | (uint)color.b << 16 | (uint)color.g << 8 | (uint)color.r;
        Point_Vert vert = {
            .x = x * r,
            .y = y * r,
            .z = z * r,
            .color = uint_color,
        };
        vk_da_append(&pc, vert);
    }

    pc.buff.count = pc.count;
    pc.buff.size  = pc.count * sizeof(*pc.items);

    return pc;
}

Frame_Buffer alloc_frame_buff(size_t width, size_t height)
{
    size_t count = width * height;
    size_t sz = sizeof(uint64_t) * count;
    Frame_Buffer frame = {
        .buff = {
            .count = count,
            .size  = sz,
        },
        .data = malloc(sz),
    };
    return frame;
}

bool setup_prerender_pass()
{
    VkAttachmentDescription color = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
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
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    };
    VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    // VkSubpassDependency dependencies[2] = {
    //     {
    //         .srcSubpass = VK_SUBPASS_EXTERNAL,
    //         .dstSubpass = 0,
    //         .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //         .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    //         .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
    //         .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    //         .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    //     },
    //     {
    //         .srcSubpass = 0,
    //         .dstSubpass = VK_SUBPASS_EXTERNAL,
    //         .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    //         .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //         .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    //         .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    //         .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    //     },
    // };
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };
    VkSubpassDescription subpass = {
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };
    VkAttachmentDescription attachments[] = {color, depth};
    VkRenderPassCreateInfo render_pass_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = VK_ARRAY_LEN(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        // .dependencyCount = VK_ARRAY_LEN(dependencies),
        // .pDependencies = dependencies,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    return vk_create_render_pass(&render_pass_ci, &prepass.rp);
}

bool setup_prepass_frame_buff(Window_Size win_sz)
{
    bool result = true;

    /* create the depth image */
    prepass.depth.img = (Vk_Image) {
        .extent = {win_sz.width, win_sz.height},
        .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .format = VK_FORMAT_D32_SFLOAT,
    };
    result = vk_img_init(
        &prepass.depth.img,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (!result) vk_return_defer(false);

    if (!vk_img_view_init(prepass.depth.img, &prepass.depth.view)) vk_return_defer(false);
    if (!vk_sampler_init(&prepass.depth.sampler))                  vk_return_defer(false);

    /* create color image */
    // uint32_t *data = malloc(win_sz.width * win_sz.height * sizeof(int));
    // // uint8_t r = 232, g = 18, b = 32;
    // uint8_t r = 255, g = 0, b = 0;
    // uint32_t color = (uint32_t)255 << 24 | (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r;
    // for (int i = 0; i < win_sz.width * win_sz.height; i++) data[i] = color;
    // if (!vk_load_texture(data, win_sz.width, win_sz.height, VK_FORMAT_R8G8B8A8_UNORM, &prepass.color))
    //     vk_return_defer(false);

    /* create the color image */
    prepass.color.img = (Vk_Image) {
        .extent = {win_sz.width, win_sz.height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    };
    result = vk_img_init(
        &prepass.color.img,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (!result) vk_return_defer(false);
    if (!vk_img_view_init(prepass.color.img, &prepass.color.view)) vk_return_defer(false);
    if (!vk_sampler_init(&prepass.color.sampler))                  vk_return_defer(false);

    /* create the frame buffer which combines both depth and color */
    prepass.img_views[0] = prepass.color.view;
    prepass.img_views[1] = prepass.depth.view;
    result = vk_create_frame_buff(
        win_sz.width,
        win_sz.height,
        prepass.img_views,
        VK_ARRAY_LEN(prepass.img_views),
        prepass.rp,
        &prepass.frame_buff
    );

defer:
    // free(data);
    return result;
}

bool setup_ds_layouts()
{
    /* mix.comp shader layout */
    VkDescriptorSetLayoutBinding mix_bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2, COMBINED_IMAGE_SAMPLER, COMPUTE_BIT)},
        {DS_BINDING(3, COMBINED_IMAGE_SAMPLER, COMPUTE_BIT)},
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = VK_ARRAY_LEN(mix_bindings),
        .pBindings = mix_bindings,
    };
    if (!vk_create_ds_layout(layout_ci, &comp_mix.ds_layout)) return false;

    /* render.comp shader layout */
    VkDescriptorSetLayoutBinding comp_render_bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2, STORAGE_BUFFER, COMPUTE_BIT)},
    };
    layout_ci.bindingCount = VK_ARRAY_LEN(comp_render_bindings);
    layout_ci.pBindings = comp_render_bindings;
    if (!vk_create_ds_layout(layout_ci, &comp_render.ds_layout)) return false;

    /* resolve.comp shader layout */
    VkDescriptorSetLayoutBinding comp_resolve_bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(2,  STORAGE_IMAGE, COMPUTE_BIT)},
    };
    layout_ci.bindingCount = VK_ARRAY_LEN(comp_resolve_bindings);
    layout_ci.pBindings = comp_resolve_bindings;
    if (!vk_create_ds_layout(layout_ci, &comp_resolve.ds_layout)) return false;

    /* texture sampler in fragment shader */
    VkDescriptorSetLayoutBinding sst_gfx_binding = {
        DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)
    };
    layout_ci.bindingCount = 1;
    layout_ci.pBindings = &sst_gfx_binding;
    if (!vk_create_ds_layout(layout_ci, &sst_gfx.ds_layout)) return false;

    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 3)},
        {DS_POOL(STORAGE_BUFFER, 4)},
        {DS_POOL(COMBINED_IMAGE_SAMPLER, 3)},
        {DS_POOL(STORAGE_IMAGE, 1)},
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = VK_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 4, // one for each pipline
    };

    if (!vk_create_ds_pool(pool_ci, &pool)) return false;

    return true;
}

bool setup_ds_sets(Vk_Buffer ubo, Vk_Buffer point_cloud, Vk_Buffer frame_buff, Vk_Texture storage_tex)
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetAllocateInfo comp_mix_alloc = {DS_ALLOC(&comp_mix.ds_layout, 1, pool)};
    if (!vk_alloc_ds(comp_mix_alloc, &comp_mix.ds)) return false;
    VkDescriptorSetAllocateInfo comp_render_alloc = {DS_ALLOC(&comp_render.ds_layout, 1, pool)};
    if (!vk_alloc_ds(comp_render_alloc, &comp_render.ds)) return false;
    VkDescriptorSetAllocateInfo comp_resolve_alloc = {DS_ALLOC(&comp_resolve.ds_layout, 1, pool)};
    if (!vk_alloc_ds(comp_resolve_alloc, &comp_resolve.ds)) return false;
    VkDescriptorSetAllocateInfo sst_gfx_alloc = {DS_ALLOC(&sst_gfx.ds_layout, 1, pool)};
    if (!vk_alloc_ds(sst_gfx_alloc, &sst_gfx.ds)) return false;

    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = ubo.handle,
        .range  = ubo.size,
    };
    VkDescriptorBufferInfo pc_info = {
        .buffer = point_cloud.handle,
        .range  = point_cloud.size,
    };
    VkDescriptorBufferInfo frame_buff_info = {
        .buffer = frame_buff.handle,
        .range  = frame_buff.size,
    };
    VkDescriptorImageInfo img_info = {
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .imageView   = storage_tex.view,
        .sampler     = storage_tex.sampler,
    };
    VkDescriptorImageInfo depth_img_info = {
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        .imageView   = prepass.depth.view,
        .sampler     = prepass.depth.sampler,
    };
    VkDescriptorImageInfo color_img_info = {
        // .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        // .imageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView   = prepass.color.view,
        .sampler     = prepass.color.sampler,
    };
    VkWriteDescriptorSet writes[] = {
        /* mix.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, comp_mix.ds, &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, comp_mix.ds, &frame_buff_info)},
        {DS_WRITE_IMG (2, COMBINED_IMAGE_SAMPLER, comp_mix.ds, &depth_img_info)},
        {DS_WRITE_IMG (3, COMBINED_IMAGE_SAMPLER, comp_mix.ds, &color_img_info)},
        /* render.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, comp_render.ds,  &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, comp_render.ds,  &pc_info)},
        {DS_WRITE_BUFF(2, STORAGE_BUFFER, comp_render.ds,  &frame_buff_info)},
        /* resolve.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, comp_resolve.ds, &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, comp_resolve.ds, &frame_buff_info)},
        {DS_WRITE_IMG (2, STORAGE_IMAGE,  comp_resolve.ds, &img_info)},
        /* sst.frag */
        {DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, sst_gfx.ds, &img_info)},
    };
    vk_update_ds(VK_ARRAY_LEN(writes), writes);

    return true;
}

bool build_compute_cmds(size_t point_cloud_count, Window_Size win_sz)
{
    size_t group_x = 1; size_t group_y = 1; size_t group_z = 1;

    /* begin recording compute commands */
    if (!vk_rec_compute()) return false;
        // vk_swapchain_img_barrier();

        /* mix the frame buffer from prepass fixed-function render */
        group_x = ceilf((float)win_sz.width / IMG_WORKGROUP_SZ);
        group_y = ceilf((float)win_sz.height / IMG_WORKGROUP_SZ);
        vk_compute(comp_mix.pl, comp_mix.pl_layout, comp_mix.ds, group_x, group_y, group_z);

        vk_compute_pl_barrier();

        /* submit batches of points to render-compute shader */
        group_x = point_cloud_count / WORKGROUP_SZ;
        size_t batch_size = group_x / NUM_BATCHES + 1;
        for (size_t i = 0; i < NUM_BATCHES; i++) {
            uint32_t offset = i * batch_size * WORKGROUP_SZ;
            vk_push_const(comp_render.pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), &offset);
            vk_compute(comp_render.pl, comp_render.pl_layout, comp_render.ds, batch_size, group_y, group_z);
        }

        vk_compute_pl_barrier();

        /* resolve the frame buffer */
        group_x = ceilf((float)win_sz.width / IMG_WORKGROUP_SZ);
        group_y = ceilf((float)win_sz.height / IMG_WORKGROUP_SZ);
        vk_compute(comp_resolve.pl, comp_resolve.pl_layout, comp_resolve.ds, group_x, group_y, group_z);

    /* end recording compute commands */
    if (!vk_end_rec_compute()) return false;

    return true;
}

bool create_prepass_pipeline()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(float16),
    };
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!vk_pl_layout_init(layout_ci, &prepass_gfx.pl_layout)) return false;

    /* create pipeline */
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(Vector3),
        },
        {
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(Vector3) * 2
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vector3) * 2 + sizeof(Vector2),
    };
    assert(prepass.rp != NULL && "prepass renderpass was NULL");
    Pipeline_Config config = {
        .pl_layout = prepass_gfx.pl_layout,
        .vert = "./res/default.vert.spv",
        .frag = "./res/default.frag.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = VK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
        .render_pass = prepass.rp,
    };
    if (!vk_basic_pl_init(config, &prepass_gfx.pl)) return false;

    return true;
}

bool create_pipelines()
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &comp_render.ds_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    /* compute shader render pipeline */
    if (!vk_pl_layout_init(layout_ci, &comp_render.pl_layout)) return false;
    if (!vk_compute_pl_init("./res/render.comp.glsl.spv", comp_render.pl_layout, &comp_render.pl)) return false;

    /* compute shader resolve pipeline */
    layout_ci.pSetLayouts = &comp_resolve.ds_layout;
    layout_ci.pushConstantRangeCount = 0;
    if (!vk_pl_layout_init(layout_ci, &comp_resolve.pl_layout)) return false;
    if (!vk_compute_pl_init("./res/resolve.comp.glsl.spv", comp_resolve.pl_layout, &comp_resolve.pl)) return false;

    /* compute mix shader render pipeline */
    layout_ci.pSetLayouts = &comp_mix.ds_layout;
    if (!vk_pl_layout_init(layout_ci, &comp_mix.pl_layout)) return false;
    if (!vk_compute_pl_init("./res/mix.comp.glsl.spv", comp_mix.pl_layout, &comp_mix.pl)) return false;

    /* screen space triangle + frag image sampler for raster display */
    layout_ci.pSetLayouts = &sst_gfx.ds_layout;
    if (!vk_pl_layout_init(layout_ci, &sst_gfx.pl_layout)) return false;
    if (!vk_sst_pl_init(sst_gfx.pl_layout, &sst_gfx.pl))   return false;

    return create_prepass_pipeline();
}

#define shift(xs_sz, xs) (assert((xs_sz) > 0), (xs_sz)--, *(xs)++)

int main(int argc, char **argv)
{
    shift(argc, argv);
    if (argc > 0) {
        const char *option = shift(argc, argv);
        if (strcmp(option, "--fullscreen") == 0) enable_full_screen();
    }

    Point_Cloud_UBO ubo = {.buff = {.count = 1, .size = sizeof(UBO_Data)}};
    Point_Cloud pc = gen_point_cloud(POINT_COUNT);
    vk_log(VK_INFO, "point count %zu", pc.count);

    /* initialize window and Vulkan */
    init_window(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "mixing rasterization with fixed function");
    Window_Size win_sz = get_window_size();
    Vk_Texture storage_tex = {.img.extent = {win_sz.width, win_sz.height}};
    Frame_Buffer frame = alloc_frame_buff(win_sz.width, win_sz.height);
    Camera camera = {
        .position   = {0.0f, 0.0f, 5.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    if (!vk_comp_buff_staged_upload(&pc.buff, pc.items))      return 1;
    if (!vk_comp_buff_staged_upload(&frame.buff, frame.data)) return 1;
    if (!vk_ubo_init(&ubo.buff))                              return 1;
    if (!vk_create_storage_img(&storage_tex))                 return 1;

    /* setup fixed prepass resources */
    if (!setup_prerender_pass()) return 1;
    if (!setup_prepass_frame_buff(win_sz)) return 1;

    /* setup descriptors */
    if (!setup_ds_layouts())                                        return 1;
    if (!setup_ds_pool())                                           return 1;
    if (!setup_ds_sets(ubo.buff, pc.buff, frame.buff, storage_tex)) return 1;

    /* create pipelines */
    if (!create_pipelines()) return 1;

    /* record commands for compute buffer */
    if (!build_compute_cmds(pc.count, win_sz)) return 1;

    Shape_Type shape = 0;
    float s = 1.0f;
    bool spin = true;
    set_target_fps(100);

    /* game loop */
    while (!window_should_close()) {
        /* input */
        update_camera_free(&camera);
        if (is_key_pressed(KEY_SPACE)) shape = (shape + 1) % SHAPE_COUNT;
        if (is_key_down(KEY_UP))   s += get_frame_time();
        if (is_key_down(KEY_DOWN)) s -= get_frame_time();
        if (is_key_pressed(KEY_ZERO)) spin = !spin;

        /* prerender pass for 3d model */
        if (!vk_wait_reset()) return 1;
        begin_mode_3d(camera);
        vk_begin_rec_gfx();
        vk_begin_offscreen_render_pass(
                1.0, 0.0, 0.0, 1.0,
                prepass.rp, prepass.frame_buff, prepass.depth.img.extent);
        if (spin) rotate_y(get_time() * 1.0);
        scale(s, s, s);
        if (!draw_shape_ex(prepass_gfx.pl, prepass_gfx.pl_layout, NULL, shape))
            return 1;
        vk_end_render_pass();
        // vk_raster_sampler_barrier(prepass.depth.img.handle);
        vk_end_rec_gfx();
        end_mode_3d();
        vk_basic_gfx_submit();

        /* submit compute commands */
        begin_mode_3d(camera);
            rotate_x(get_time() * 0.5);
            float donut_scale = sinf(get_time() * 0.5) + 1.5;
            scale(donut_scale, donut_scale, donut_scale);
            vk_compute_fence_wait();
            if (get_mvp_float16(&ubo.data.mvp)) {
                ubo.data.width  = win_sz.width;
                ubo.data.height = win_sz.height;
                memcpy(ubo.buff.mapped, &ubo.data, ubo.buff.size);
            } else {
                return 1;
            }
            if (!vk_submit_compute()) return 1;
        end_mode_3d();

        /* draw command for screen space triangle (sst) */
        begin_timer();
        if (!vk_wait_to_begin_gfx()) return 1;
            vk_begin_rec_gfx();
                vk_raster_sampler_barrier(storage_tex.img.handle);
                vk_begin_render_pass(0.0f, 0.0f, 0.0f, 1.0f);
                    vk_draw_sst(sst_gfx.pl, sst_gfx.pl_layout, sst_gfx.ds);
                vk_end_render_pass();
            vk_end_rec_gfx();
        vk_submit_gfx();
        // goto defer;

        end_timer();
        poll_input_events();
    }

// defer:
    wait_idle();
    free(pc.items);
    free(frame.data);
    vk_buff_destroy(&pc.buff);
    vk_buff_destroy(&frame.buff);
    vk_buff_destroy(&ubo.buff);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(comp_mix.ds_layout);
    vk_destroy_ds_layout(comp_render.ds_layout);
    vk_destroy_ds_layout(comp_resolve.ds_layout);
    vk_destroy_ds_layout(sst_gfx.ds_layout);
    vk_destroy_pl_res(comp_mix.pl, comp_mix.pl_layout);
    vk_destroy_pl_res(comp_render.pl, comp_render.pl_layout);
    vk_destroy_pl_res(comp_resolve.pl, comp_resolve.pl_layout);
    vk_destroy_pl_res(sst_gfx.pl, sst_gfx.pl_layout);
    vk_destroy_pl_res(prepass_gfx.pl, prepass_gfx.pl_layout);
    vk_unload_texture(&storage_tex);
    vk_unload_texture(&prepass.depth);
    vk_unload_texture(&prepass.color);
    vk_destroy_frame_buff(prepass.frame_buff);
    vk_destroy_render_pass(prepass.rp);
    close_window();
    return 0;
}
