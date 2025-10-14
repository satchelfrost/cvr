#include "cvr.h"

#define NUM_POINTS 1000*1000*10
#define WORKGROUP_SZ 1024
#define NUM_BATCHES 8

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
    const size_t max;
    const size_t min;
} Point_Cloud;

typedef struct {
    Rvk_Buffer buff;
    void *data;
} Frame_Buffer;

typedef struct {
    float16 mvp;
} UBO_Data;

typedef struct {
    Rvk_Buffer buff;
    UBO_Data data;
} Point_Cloud_UBO;

Rvk_Descriptor_Pool_Arena arena = {0};

typedef struct {
    VkPipelineLayout layout;
    VkPipeline pl;
    Rvk_Descriptor_Set_Layout ds_layout;
    VkDescriptorSet ds;
} Pipeline;

Pipeline cs_render = {0};
Pipeline cs_resolve = {0};
Pipeline gfx = {0};

void gen_points(size_t num_points, Point_Cloud *pc)
{
    /* reset the point count to zero, but leave capacity allocated */
    pc->count = 0;

    printf("generating points...\n");
    for (size_t i = 0; i < num_points; i++) {
        float theta = PI * rand() / RAND_MAX;
        float phi   = 2.0f * PI * rand() / RAND_MAX;
        float r     = 10.0f * rand() / RAND_MAX;
        Color color = color_from_HSV(r * 360.0f, 1.0f, 1.0f);
        uint uint_color = (uint)color.a << 24 | (uint)color.b << 16 | (uint)color.g << 8 | (uint)color.r;
        Point_Vert vert = {
            .x = r * sin(theta) * cos(phi),
            .y = r * sin(theta) * sin(phi),
            .z = r * cos(theta),
            .color = uint_color,
        };

        rvk_da_append(pc, vert);
    }

    printf("done generating points.\n");
    pc->buff.count = pc->count;
    pc->buff.size  = pc->count * sizeof(*pc->items);
}

Frame_Buffer alloc_frame_buff()
{
    Frame_Buffer frame = {0};
    frame.buff.count = 1600 * 900;
    frame.buff.size  = sizeof(uint64_t) * frame.buff.count;
    frame.data = malloc(frame.buff.size);
    return frame;
}

void setup_ds_layouts()
{
    /* render.comp shader layout */
    VkDescriptorSetLayoutBinding cs_render_bindings[] = {
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
        }
    };
    rvk_ds_layout_init(cs_render_bindings, RVK_ARRAY_LEN(cs_render_bindings), &cs_render.ds_layout);

    /* resolve.comp shader layout */
    VkDescriptorSetLayoutBinding cs_resolve_bindings[] = {
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
        }
    };
    rvk_ds_layout_init(cs_resolve_bindings, RVK_ARRAY_LEN(cs_resolve_bindings), &cs_resolve.ds_layout);

    /* texture sampler in fragment shader */
    VkDescriptorSetLayoutBinding gfx_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    rvk_ds_layout_init(&gfx_binding, 1, &gfx.ds_layout);
}

bool setup_ds_sets(Rvk_Buffer ubo, Rvk_Buffer point_cloud, Rvk_Buffer frame_buff, Rvk_Texture storage_tex)
{
    /* allocate descriptor sets based on layouts */
    rvk_descriptor_pool_arena_alloc_set(&arena, &cs_render.ds_layout,  &cs_render.ds);
    rvk_descriptor_pool_arena_alloc_set(&arena, &cs_resolve.ds_layout, &cs_resolve.ds);
    rvk_descriptor_pool_arena_alloc_set(&arena, &gfx.ds_layout,     &gfx.ds);

    /* update descriptor sets */
    VkWriteDescriptorSet writes[] = {
        /* render.comp */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet = cs_render.ds,
            .pBufferInfo = &ubo.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = cs_render.ds,
            .pBufferInfo = &point_cloud.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = cs_render.ds,
            .pBufferInfo = &frame_buff.info,
        },
        /* resolve.comp */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet = cs_resolve.ds,
            .pBufferInfo = &ubo.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = cs_resolve.ds,
            .pBufferInfo = &frame_buff.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .dstSet = cs_resolve.ds,
            .pImageInfo = &storage_tex.info,
        },
        /* default.frag */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .dstSet = gfx.ds,
            .pImageInfo = &storage_tex.info,
        },
    };
    rvk_update_ds(RVK_ARRAY_LEN(writes), writes);

    return true;
}

void build_compute_cmds(size_t point_cloud_count)
{
    size_t group_x = 1; size_t group_y = 1; size_t group_z = 1;

    /* submit batches of points to render-compute shader */
    group_x = ceilf((float)point_cloud_count / WORKGROUP_SZ);
    size_t batch_size = ceilf((float)group_x / NUM_BATCHES);
    for (size_t i = 0; i < NUM_BATCHES; i++) {
        uint32_t offset = i * batch_size * WORKGROUP_SZ;
        rvk_push_const(cs_render.layout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), &offset);
        rvk_dispatch(cs_render.pl , cs_render.layout, cs_render.ds, batch_size, group_y, group_z);
    }

    rvk_compute_pl_barrier();

    /* resolve the frame buffer */
    group_x = ceilf(1600 / 16.0f);
    group_y = ceilf(900 / 16.0f);
    rvk_dispatch(cs_resolve.pl, cs_resolve.layout, cs_resolve.ds, group_x, group_y, group_z);
}

void create_pipelines()
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(uint32_t)};

    /* compute shader render pipeline */
    rvk_create_pipeline_layout(
        &cs_render.layout,
        .p_push_constant_ranges = &pk_range,
        .push_constant_range_count = 1,
        .p_set_layouts = &cs_render.ds_layout.handle
    );
    rvk_compute_pl_init("./res/render.comp.glsl.spv", cs_render.layout, &cs_render.pl);

    /* compute shader resolve pipeline */
    rvk_create_pipeline_layout(
        &cs_resolve.layout,
        .p_set_layouts = &cs_resolve.ds_layout.handle
    );
    rvk_compute_pl_init("./res/resolve.comp.glsl.spv", cs_resolve.layout, &cs_resolve.pl);

    /* screen space triangle + frag image sampler for raster display */
    rvk_create_pipeline_layout(
        &gfx.layout,
        .p_set_layouts = &gfx.ds_layout.handle
    );
    rvk_sst_pl_init(gfx.layout, &gfx.pl);
}

int main()
{
    Point_Cloud pc = {0};
    Rvk_Texture storage_tex = {.img.extent = {1600, 900}};
    Frame_Buffer frame = alloc_frame_buff();
    Point_Cloud_UBO ubo = {0};

    /* generate initial point cloud */
    gen_points(NUM_POINTS, &pc);

    /* initialize window and Vulkan */
    rvk_enable_atomic_features();
    init_window(1600, 900, "compute based rasterization for a point cloud");
    Camera camera = {
        .position   = {10.0f, 10.0f, 10.0f},
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

    /* setup descriptors */
    rvk_descriptor_pool_arena_init(&arena);
    setup_ds_layouts();
    setup_ds_sets(ubo.buff, pc.buff, frame.buff, storage_tex);

    /* create pipelines */
    create_pipelines();

    /* game loop */
    while (!window_should_close()) {
        /* input */
        if (is_key_pressed(KEY_F)) log_fps();

        /* submit compute commands */
        begin_frame();
        begin_mode_3d(camera);
            rotate_y(get_time() * 0.5);
            get_mvp_float16(&ubo.data.mvp);
            memcpy(ubo.buff.mapped, &ubo.data, ubo.buff.size);
            build_compute_cmds(pc.count);
        end_mode_3d();

        rvk_raster_sampler_barrier(storage_tex.img.handle);

        /* draw command for screen space triangle (sst) */
        rvk_begin_render_pass(0.0f, 0.0f, 0.0f, 1.0f);
            rvk_draw_sst(gfx.pl, gfx.layout, gfx.ds);
        end_drawing(); // ends gfx rec commands and render pass
    }

    rvk_wait_idle();
    free(pc.items);
    free(frame.data);
    rvk_buff_destroy(pc.buff);
    rvk_buff_destroy(frame.buff);
    rvk_buff_destroy(ubo.buff);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_destroy_descriptor_set_layout(cs_render.ds_layout.handle);
    rvk_destroy_descriptor_set_layout(cs_resolve.ds_layout.handle);
    rvk_destroy_descriptor_set_layout(gfx.ds_layout.handle);
    rvk_destroy_pl_res(cs_render.pl, cs_render.layout);
    rvk_destroy_pl_res(cs_resolve.pl, cs_resolve.layout);
    rvk_destroy_pl_res(gfx.pl, gfx.layout);
    rvk_unload_texture(storage_tex);
    close_window();
    return 0;
}
