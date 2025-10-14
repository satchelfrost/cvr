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
    Rvk_Buffer buff;
} Point_Cloud;

typedef struct {
    Rvk_Buffer buff;
    void *data;
} Frame_Buffer;

typedef struct {
    float16 mvp;
    int width;
    int height;
} UBO_Data;

typedef struct {
    Rvk_Buffer buff;
    UBO_Data data;
} Point_Cloud_UBO;

typedef struct {
    VkFramebuffer frame_buff;
    Rvk_Texture depth;
    Rvk_Texture color;
    VkRenderPass rp;
    VkImageView img_views[2];
} Prepass;

typedef struct {
    VkPipeline pl;
    VkPipelineLayout pl_layout;
    Rvk_Descriptor_Set_Layout ds_layout;
    VkDescriptorSet ds;
} Pipeline;

Rvk_Descriptor_Pool_Arena arena = {0};

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
        rvk_da_append(&pc, vert);
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

void setup_prerender_pass()
{
    VkAttachmentDescription color = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
    VkSubpassDependency dependency = {
        // .srcSubpass = VK_SUBPASS_EXTERNAL,
        // .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        // .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        // .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,

        .srcSubpass      = VK_SUBPASS_EXTERNAL,
        .dstSubpass      = 0,
        .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask   = 0,
        .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,

    };
    VkSubpassDescription subpass = {
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };
    VkAttachmentDescription attachments[] = {color, depth};
    VkRenderPassCreateInfo render_pass_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = RVK_ARRAY_LEN(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    rvk_create_render_pass(&render_pass_ci, &prepass.rp);
}

void setup_prepass_frame_buff(Window_Size win_sz)
{
    /* create the depth image */
    prepass.depth.img = (Rvk_Image) {
        .extent = {win_sz.width, win_sz.height},
        .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .format = VK_FORMAT_D32_SFLOAT,
    };
    rvk_img_init(
        &prepass.depth.img,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    rvk_img_view_init(prepass.depth.img, &prepass.depth.view);
    rvk_sampler_init(&prepass.depth.sampler);

    /* book keeping */
    prepass.depth.info = (VkDescriptorImageInfo) {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .sampler = prepass.depth.sampler,
        .imageView = prepass.depth.view,
    };

    /* create the color image */
    prepass.color.img = (Rvk_Image) {
        .extent = {win_sz.width, win_sz.height},
        .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    };
    rvk_img_init(
        &prepass.color.img,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    rvk_img_view_init(prepass.color.img, &prepass.color.view);
    rvk_sampler_init(&prepass.color.sampler);

    /* create the frame buffer which combines both depth and color */
    prepass.img_views[0] = prepass.color.view;
    prepass.img_views[1] = prepass.depth.view;
    rvk_create_frame_buff(
        win_sz.width,
        win_sz.height,
        prepass.img_views,
        RVK_ARRAY_LEN(prepass.img_views),
        prepass.rp,
        &prepass.frame_buff
    );

    /* book keeping */
    prepass.color.info = (VkDescriptorImageInfo) {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .sampler = prepass.color.sampler,
        .imageView = prepass.color.view,
    };
}

void setup_ds_layouts()
{
    /* mix.comp shader layout */
    VkDescriptorSetLayoutBinding mix_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        }
    };
    rvk_ds_layout_init(mix_bindings, RVK_ARRAY_LEN(mix_bindings), &comp_mix.ds_layout);


    /* render.comp shader layout */
    VkDescriptorSetLayoutBinding comp_render_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    rvk_ds_layout_init(comp_render_bindings, RVK_ARRAY_LEN(comp_render_bindings), &comp_render.ds_layout);

    /* resolve.comp shader layout */
    VkDescriptorSetLayoutBinding comp_resolve_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    rvk_ds_layout_init(comp_resolve_bindings, RVK_ARRAY_LEN(comp_resolve_bindings), &comp_resolve.ds_layout);

    /* texture sampler in fragment shader */
    VkDescriptorSetLayoutBinding sst_gfx_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    rvk_ds_layout_init(sst_gfx_bindings, RVK_ARRAY_LEN(sst_gfx_bindings), &sst_gfx.ds_layout);
}

void setup_ds_sets(Rvk_Buffer ubo, Rvk_Buffer point_cloud, Rvk_Buffer frame_buff, Rvk_Texture storage_tex)
{
    /* allocate descriptor sets based on layouts */
    rvk_descriptor_pool_arena_alloc_set(&arena, &comp_mix.ds_layout,  &comp_mix.ds);
    rvk_descriptor_pool_arena_alloc_set(&arena, &comp_render.ds_layout,  &comp_render.ds);
    rvk_descriptor_pool_arena_alloc_set(&arena, &comp_resolve.ds_layout,  &comp_resolve.ds);
    rvk_descriptor_pool_arena_alloc_set(&arena, &sst_gfx.ds_layout,  &sst_gfx.ds);

    // .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    VkWriteDescriptorSet writes[] = {
        /* mix.comp */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet = comp_mix.ds,
            .pBufferInfo = &ubo.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = comp_mix.ds,
            .pBufferInfo = &frame_buff.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .dstSet = comp_mix.ds,
            .pImageInfo = &prepass.depth.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .dstSet = comp_mix.ds,
            .pImageInfo = &prepass.color.info,
        },
        /* render.comp */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet = comp_render.ds,
            .pBufferInfo = &ubo.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = comp_render.ds,
            .pBufferInfo = &point_cloud.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = comp_render.ds,
            .pBufferInfo = &frame_buff.info,
        },
        /* resolve.comp */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet = comp_resolve.ds,
            .pBufferInfo = &ubo.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = comp_resolve.ds,
            .pBufferInfo = &frame_buff.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .dstSet = comp_resolve.ds,
            .pImageInfo = &storage_tex.info,
        },
        /* sst.frag */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .dstSet = sst_gfx.ds,
            .pImageInfo = &storage_tex.info,
        },
    };
    rvk_update_ds(RVK_ARRAY_LEN(writes), writes);
}

void build_compute_cmds(size_t point_cloud_count, Window_Size win_sz)
{
    size_t group_x = 1; size_t group_y = 1; size_t group_z = 1;

    /* mix the frame buffer from prepass fixed-function render */
    group_x = ceilf((float)win_sz.width / IMG_WORKGROUP_SZ);
    group_y = ceilf((float)win_sz.height / IMG_WORKGROUP_SZ);
    rvk_dispatch(comp_mix.pl, comp_mix.pl_layout, comp_mix.ds, group_x, group_y, group_z);

    rvk_compute_pl_barrier();

    /* submit batches of points to render-compute shader */
    group_x = point_cloud_count / WORKGROUP_SZ;
    size_t batch_size = group_x / NUM_BATCHES + 1;
    for (size_t i = 0; i < NUM_BATCHES; i++) {
        uint32_t offset = i * batch_size * WORKGROUP_SZ;
        rvk_push_const(comp_render.pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), &offset);
        rvk_dispatch(comp_render.pl, comp_render.pl_layout, comp_render.ds, batch_size, group_y, group_z);
    }

    rvk_compute_pl_barrier();

    /* resolve the frame buffer */
    group_x = ceilf((float)win_sz.width / IMG_WORKGROUP_SZ);
    group_y = ceilf((float)win_sz.height / IMG_WORKGROUP_SZ);
    rvk_dispatch(comp_resolve.pl, comp_resolve.pl_layout, comp_resolve.ds, group_x, group_y, group_z);
}

void create_prepass_pipeline()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(float16),
    };
    rvk_create_pipeline_layout(
        &prepass_gfx.pl_layout,
        .p_push_constant_ranges = &pk_range,
    );

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
        .vert = "./res/default.vert.glsl.spv",
        .frag = "./res/default.frag.glsl.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = RVK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
        .render_pass = prepass.rp,
    };
    rvk_basic_pl_init(config, &prepass_gfx.pl);
}

void create_pipelines()
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(uint32_t)};
    /* compute shader render pipeline */
    rvk_create_pipeline_layout(
        &comp_render.pl_layout,
        .p_push_constant_ranges = &pk_range,
        .p_set_layouts = &comp_render.ds_layout.handle
    );
    rvk_compute_pl_init("./res/render.comp.glsl.spv", comp_render.pl_layout, &comp_render.pl);

    /* compute shader resolve pipeline */
    rvk_create_pipeline_layout(&comp_resolve.pl_layout, .p_set_layouts = &comp_resolve.ds_layout.handle);
    rvk_compute_pl_init("./res/resolve.comp.glsl.spv", comp_resolve.pl_layout, &comp_resolve.pl);

    /* compute mix shader render pipeline */
    rvk_create_pipeline_layout( &comp_mix.pl_layout, .p_set_layouts = &comp_mix.ds_layout.handle);
    rvk_compute_pl_init("./res/mix.comp.glsl.spv", comp_mix.pl_layout, &comp_mix.pl);

    /* screen space triangle + frag image sampler for raster display */
    rvk_create_pipeline_layout(&sst_gfx.pl_layout, .p_set_layouts = &sst_gfx.ds_layout.handle);
    rvk_sst_pl_init(sst_gfx.pl_layout, &sst_gfx.pl);

    create_prepass_pipeline();
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
    rvk_log(RVK_INFO, "point count %zu", pc.count);

    /* initialize window and Vulkan */
    rvk_enable_atomic_features();
    init_window(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "mixing rasterization with fixed function");
    Window_Size win_sz = get_window_size();
    Rvk_Texture storage_tex = {.img.extent = {win_sz.width, win_sz.height}};
    Frame_Buffer frame = alloc_frame_buff(win_sz.width, win_sz.height);
    Camera camera = {
        .position   = {0.0f, 0.0f, 5.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* upload resources to GPU */
    pc.buff    = rvk_upload_compute_buff(pc.buff.size, pc.buff.count, pc.items);
    frame.buff = rvk_upload_compute_buff(frame.buff.size, frame.buff.count, frame.data);
    ubo.buff   = rvk_create_mapped_uniform_buff(sizeof(UBO_Data), &ubo.data);
    rvk_storage_tex_init(&storage_tex, storage_tex.img.extent);

    /* setup vulkan resources */
    setup_prerender_pass();
    setup_prepass_frame_buff(win_sz);
    setup_ds_layouts();
    rvk_descriptor_pool_arena_init(&arena);
    setup_ds_sets(ubo.buff, pc.buff, frame.buff, storage_tex);
    create_pipelines();

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
        begin_timer();
        rvk_wait_to_begin_gfx();
        rvk_begin_rec_gfx();
            begin_mode_3d(camera);
                /* draw spinning cube in an offscreen pass */ 
                rvk_begin_offscreen_render_pass(1.0, 0.0, 0.0, 1.0, prepass.rp, prepass.frame_buff, prepass.depth.img.extent);
                    if (spin) rotate_y(get_time() * 1.0);
                    scale(s, s, s);
                draw_shape_ex(prepass_gfx.pl, prepass_gfx.pl_layout, NULL, shape);
                rvk_end_render_pass();

                rvk_depth_img_barrier(prepass.depth.img.handle);
                rvk_color_img_barrier(prepass.color.img.handle);

                /* rasterized point cloud donut thingy */
                build_compute_cmds(pc.count, win_sz);
                rotate_x(get_time() * 0.5);
                float donut_scale = sinf(get_time() * 0.5) + 1.5;
                scale(donut_scale, donut_scale, donut_scale);
                get_mvp_float16(&ubo.data.mvp);
                ubo.data.width  = win_sz.width;
                ubo.data.height = win_sz.height;
                memcpy(ubo.buff.mapped, &ubo.data, ubo.buff.size);
            end_mode_3d();

            rvk_raster_sampler_barrier(storage_tex.img.handle);

            /* draw command for screen space triangle (sst) */
            rvk_begin_render_pass(0.0f, 0.0f, 0.0f, 1.0f);
                rvk_draw_sst(sst_gfx.pl, sst_gfx.pl_layout, sst_gfx.ds);
            rvk_end_render_pass();
        rvk_end_rec_gfx();
        rvk_submit_gfx();

        end_timer();
        poll_input_events();
    }

    rvk_wait_idle();
    free(pc.items);
    free(frame.data);
    rvk_buff_destroy(pc.buff);
    rvk_buff_destroy(frame.buff);
    rvk_buff_destroy(ubo.buff);
    rvk_destroy_ds_pool(pool);
    rvk_destroy_descriptor_set_layout(comp_mix.ds_layout.handle);
    rvk_destroy_descriptor_set_layout(comp_render.ds_layout.handle);
    rvk_destroy_descriptor_set_layout(comp_resolve.ds_layout.handle);
    rvk_destroy_descriptor_set_layout(sst_gfx.ds_layout.handle);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_destroy_pl_res(comp_mix.pl, comp_mix.pl_layout);
    rvk_destroy_pl_res(comp_render.pl, comp_render.pl_layout);
    rvk_destroy_pl_res(comp_resolve.pl, comp_resolve.pl_layout);
    rvk_destroy_pl_res(sst_gfx.pl, sst_gfx.pl_layout);
    rvk_destroy_pl_res(prepass_gfx.pl, prepass_gfx.pl_layout);
    rvk_unload_texture(storage_tex);
    rvk_unload_texture(prepass.depth);
    rvk_unload_texture(prepass.color);
    rvk_destroy_frame_buff(prepass.frame_buff);
    rvk_destroy_render_pass(prepass.rp);
    close_window();
    return 0;
}
