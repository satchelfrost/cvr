#include "cvr.h"

#define MAX_POINTS 100000000 // 100 million
#define MIN_POINTS 100000    // 100 thousand
#define MAX_FPS_REC 100
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
    bool pending_change;
    const size_t max;
    const size_t min;
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
    VkRenderPass rp;
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

void gen_points(size_t num_points, Point_Cloud *pc)
{
    /* reset the point count to zero, but leave capacity allocated */
    pc->count = 0;

    for (size_t i = 0; i < num_points; i++) {
        float theta = PI * rand() / RAND_MAX;
        float phi   = 2.0f * PI * rand() / RAND_MAX;
        float r     = 1.0f * rand() / RAND_MAX;
        Color color = color_from_HSV(r * 360.0f, 1.0f, 1.0f);
        uint uint_color = (uint)color.a << 24 | (uint)color.b << 16 | (uint)color.g << 8 | (uint)color.r;
        Point_Vert vert = {
            .x = r * sin(theta) * cos(phi),
            .y = r * sin(theta) * sin(phi),
            .z = r * cos(theta),
            .color = uint_color,
        };

        vk_da_append(pc, vert);
    }

    pc->buff.count = pc->count;
    pc->buff.size  = pc->count * sizeof(*pc->items);
}

Frame_Buffer alloc_frame_buff(size_t width, size_t height)
{
    Frame_Buffer frame = {0};
    frame.buff.count = width * height;
    frame.buff.size  = sizeof(uint64_t) * frame.buff.count;
    frame.data = malloc(frame.buff.size);
    return frame;
}

bool setup_prerender_pass()
{
    VkAttachmentDescription attach_desc = {
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
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .pDepthStencilAttachment = &depth_ref,
    };
    VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };
    VkRenderPassCreateInfo render_pass_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attach_desc,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = VK_ARRAY_LEN(dependencies),
        .pDependencies = dependencies,
    };

    return vk_create_render_pass(&render_pass_ci, &prepass.rp);
}

bool setup_prepass_frame_buff()
{
    /* create the image */
    prepass.depth.img = (Vk_Image) {
        .extent = {DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT},
        .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .format = VK_FORMAT_D32_SFLOAT,
    };
    bool result = vk_img_init(
        &prepass.depth.img,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (!result) return false;

    if (!vk_img_view_init(prepass.depth.img, &prepass.depth.view)) return false;
    if (!vk_sampler_init(&prepass.depth.sampler))                  return false;

    result = vk_create_frame_buff(
        prepass.depth.img.extent.width,
        prepass.depth.img.extent.height,
        &prepass.depth.view,
        1,
        prepass.rp,
        &prepass.frame_buff
    );

    return result;
}

bool setup_ds_layouts()
{
    /* mix.comp shader layout */
    VkDescriptorSetLayoutBinding mix_bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
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
        {DS_POOL(COMBINED_IMAGE_SAMPLER, 1)},
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
    VkWriteDescriptorSet writes[] = {
        /* mix.comp */
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, comp_mix.ds, &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, comp_mix.ds, &frame_buff_info)},
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

bool build_compute_cmds(size_t point_cloud_count)
{
    size_t group_x = 1; size_t group_y = 1; size_t group_z = 1;

    /* begin recording compute commands */
    if (!vk_rec_compute()) return false;

        /* mix the frame buffer from prepass fixed-function render */
        group_x = ceilf((float)DEFAULT_WINDOW_WIDTH  / IMG_WORKGROUP_SZ);
        group_y = ceilf((float)DEFAULT_WINDOW_HEIGHT / IMG_WORKGROUP_SZ);
        vk_compute(comp_mix.pl, comp_mix.pl_layout, comp_mix.ds, group_x, group_y, group_z);

        vk_compute_pl_barrier();

        /* submit batches of points to render-compute shader */
        group_x = point_cloud_count / WORKGROUP_SZ + 1;
        size_t batch_size = group_x / NUM_BATCHES;
        for (size_t i = 0; i < NUM_BATCHES; i++) {
            uint32_t offset = i * batch_size * WORKGROUP_SZ;
            vk_push_const(comp_render.pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), &offset);
            vk_compute(comp_render.pl, comp_render.pl_layout, comp_render.ds, batch_size, group_y, group_z);
        }

        vk_compute_pl_barrier();

        /* resolve the frame buffer */
        group_x = ceilf((float)DEFAULT_WINDOW_WIDTH  / IMG_WORKGROUP_SZ);
        group_y = ceilf((float)DEFAULT_WINDOW_HEIGHT / IMG_WORKGROUP_SZ);
        vk_compute(comp_resolve.pl, comp_resolve.pl_layout, comp_resolve.ds, group_x, group_y, group_z);

    /* end recording compute commands */
    if (!vk_end_rec_compute()) return false;

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
    if (!vk_pl_layout_init(layout_ci, &comp_render.pl_layout))                                return false;
    if (!vk_compute_pl_init("./res/render.comp.glsl.spv", comp_render.pl_layout, &comp_render.pl)) return false;

    /* compute shader resolve pipeline */
    layout_ci.pSetLayouts = &comp_resolve.ds_layout;
    layout_ci.pushConstantRangeCount = 0;
    if (!vk_pl_layout_init(layout_ci, &comp_resolve.pl_layout))                                  return false;
    if (!vk_compute_pl_init("./res/resolve.comp.glsl.spv", comp_resolve.pl_layout, &comp_resolve.pl)) return false;

    /* compute mix shader render pipeline */
    layout_ci.pSetLayouts = &comp_mix.ds_layout;
    if (!vk_pl_layout_init(layout_ci, &comp_mix.pl_layout))                          return false;
    if (!vk_compute_pl_init("./res/mix.comp.glsl.spv", comp_mix.pl_layout, &comp_mix.pl)) return false;

    /* screen space triangle + frag image sampler for raster display */
    layout_ci.pSetLayouts = &sst_gfx.ds_layout;
    if (!vk_pl_layout_init(layout_ci, &sst_gfx.pl_layout)) return false;
    if (!vk_sst_pl_init(sst_gfx.pl_layout, &sst_gfx.pl))   return false;

    return true;
}

int main()
{
    Point_Cloud pc = {.min = MIN_POINTS, .max = MAX_POINTS};
    Vk_Texture storage_tex = {.img.extent = {DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT}};
    Frame_Buffer frame = alloc_frame_buff(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    Point_Cloud_UBO ubo = {.buff = {.count = 1, .size = sizeof(UBO_Data)}};

    /* generate initial point cloud */
    size_t num_points = MIN_POINTS;
    gen_points(num_points, &pc);

    /* initialize window and Vulkan */
    init_window(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "mixing rasterization with fixed function");
    Camera camera = {
        .position   = {1.0f, 1.0f, 1.0f},
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
    if (!setup_prepass_frame_buff()) return 1;

    /* setup descriptors */
    if (!setup_ds_layouts())                                        return 1;
    if (!setup_ds_pool())                                           return 1;
    if (!setup_ds_sets(ubo.buff, pc.buff, frame.buff, storage_tex)) return 1;

    /* create pipelines */
    if (!create_pipelines()) return 1;

    /* record commands for compute buffer */
    if (!build_compute_cmds(pc.count)) return 1;

    /* game loop */
    while (!window_should_close()) {
        /* input */
        update_camera_free(&camera);
        if (is_key_pressed(KEY_UP) || is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
            if (num_points * 10 <= pc.max) {
                pc.pending_change = true;
                num_points = num_points * 10;
            } else {
                vk_log(VK_INFO, "max point count reached");
            }
        }
        if (is_key_pressed(KEY_DOWN) || is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
            if (num_points / 10 >= pc.min) {
                pc.pending_change = true;
                num_points = num_points / 10;
            } else {
                vk_log(VK_INFO, "min point count reached");
            }
        }

        /* re-upload point cloud if we've changed point cloud size */
        if (pc.pending_change) {
            /* destroy old point cloud buffer and generate new points */
            wait_idle();
            vk_buff_destroy(&pc.buff);
            gen_points(num_points, &pc);

            /* upload new buffer and update descriptor sets */
            if (!vk_comp_buff_staged_upload(&pc.buff, pc.items)) return 1;
            VkDescriptorBufferInfo pc_info = {.buffer = pc.buff.handle, .range = pc.buff.size};
            VkWriteDescriptorSet write = {DS_WRITE_BUFF(1, STORAGE_BUFFER, comp_render.ds,  &pc_info)};
            vk_update_ds(1, &write);

            /* rebuild compute commands */
            if (!build_compute_cmds(pc.count)) return 1;
            pc.pending_change = false;
        }

        /* prerender pass for 3d model */

        /* submit compute commands */
        begin_mode_3d(camera);
            rotate_y(get_time() * 0.5);
            vk_compute_fence_wait();
            if (get_mvp_float16(&ubo.data.mvp)) {
                ubo.data.width  = DEFAULT_WINDOW_WIDTH;
                ubo.data.height = DEFAULT_WINDOW_HEIGHT;
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
        end_drawing(); // ends gfx rec commands and render pass
    }

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
    vk_unload_texture(&storage_tex);
    vk_unload_texture(&prepass.depth);
    vk_destroy_frame_buff(prepass.frame_buff);
    vk_destroy_render_pass(prepass.rp);
    close_window();
    return 0;
}
