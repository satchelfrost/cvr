#include "cvr.h"
#include "geometry.h"

#define PARTICLE_COUNT 512

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    Vector4 color;
} Particle;

static Particle particles[PARTICLE_COUNT];

/* Vulkan state */
Rvk_Descriptor_Pool_Arena arena = {0};
Rvk_Descriptor_Set_Layout compute_ds_layout = {0};
VkDescriptorSet compute_ds;

VkPipelineLayout compute_pl_layout;
VkPipeline compute_pl;
VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

Color colors[] = {
    LIGHTGRAY, GRAY, DARKGRAY, YELLOW, GOLD, ORANGE, PINK, RED,
    MAROON, GREEN, LIME, DARKGREEN, SKYBLUE, BLUE, DARKBLUE,
    PURPLE, VIOLET, DARKPURPLE, BEIGE, BROWN, DARKBROWN, WHITE,
    BLACK, BLANK, MAGENTA, RAYWHITE
};

Vector4 color_to_vec4(Color c)
{
    return (Vector4){c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0};
}

void init_particles(Particle *particles, float width, float height)
{
    for (size_t i = 0; i < PARTICLE_COUNT; i++) {
        float r = (float) rand() / RAND_MAX;
        float theta = (2 * i % 360) * (3.14259f / 180.0f);
        Particle *particle = &particles[i];
        float x = r * cos(theta) * height / width;
        float y = r * sin(theta);
        particle->pos.x = x;
        particle->pos.y = y;
        particle->velocity = Vector2Normalize(particle->pos);
        particle->velocity.x *= 0.0025f;
        particle->velocity.y *= 0.0025f;
        particle->color = color_to_vec4(colors[i % RVK_ARRAY_LEN(colors)]);
    }
}

void setup_ds_layout()
{
    VkDescriptorSetLayoutBinding bindings[] = {
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
        }
    };
    rvk_ds_layout_init(bindings, RVK_ARRAY_LEN(bindings), &compute_ds_layout);
}

void setup_ds(Rvk_Buffer ubo, Rvk_Buffer comp_buff)
{
    /* allocate descriptor sets based on layouts */
    rvk_descriptor_pool_arena_alloc_set(&arena, &compute_ds_layout, &compute_ds);

    /* update descriptor sets */
    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstSet = compute_ds,
            .pBufferInfo = &ubo.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .dstSet = compute_ds,
            .pBufferInfo = &comp_buff.info,
        },
    };
    rvk_update_ds(RVK_ARRAY_LEN(writes), writes);
}

void create_pipelines()
{
    /* compute pipeline to handle velocities of particles */
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &compute_ds_layout.handle,
    };
    rvk_pl_layout_init(layout_ci, &compute_pl_layout);
    rvk_compute_pl_init("./res/velocity.comp.glsl.spv", compute_pl_layout, &compute_pl);

    /* pipeline to render points */
    rvk_create_pipeline_layout(&gfx_pl_layout);
    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = offsetof(Particle, pos),  },
        { .location = 1, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Particle, color),},
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Particle),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = RVK_ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_POINT,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
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
    rvk_create_graphics_pipelines(
        &gfx_pl,
        .vertex_shader_name = "./res/particle.vert.glsl.spv",
        .fragment_shader_name = "./res/particle.frag.glsl.spv",
        .p_vertex_input_state = &vertex_input_ci,
        .layout = gfx_pl_layout,
        .p_input_assembly_state = &input_assembly_ci,
        .p_rasterization_state = &rasterizer_ci,
        .p_color_blend_state = &color_blend_ci,
    );
}

int main()
{
    float width  = 800.0f;
    float height = 600.0f;
    init_particles(particles, width, height);

    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    /* init window and Vulkan */
    init_window(width, height, "Compute Shader");

    /* initialize some vulkan resources */
    Rvk_Buffer comp_buff = rvk_upload_compute_buff(PARTICLE_COUNT * sizeof(Particle), PARTICLE_COUNT, particles);
    float time = 0.0f;
    Rvk_Buffer ubo = rvk_create_mapped_uniform_buff(sizeof(float), &time);
    rvk_descriptor_pool_arena_init(&arena);
    setup_ds_layout();
    setup_ds(ubo, comp_buff);
    create_pipelines();

    set_target_fps(60);
    while (!window_should_close()) {
        begin_frame();
        /* record compute commands */
        size_t group_x = ceilf(PARTICLE_COUNT / 256.0f);
        rvk_dispatch(compute_pl, compute_pl_layout, compute_ds, group_x, 1, 1);

        rvk_begin_render_pass(0.0f, 0.0f, 0.0f, 1.0f);
            /* record drawing commands */
            begin_mode_3d(camera);
                rvk_bind_gfx(gfx_pl, gfx_pl_layout, NULL, 0);
                rvk_bind_vertex_buffers(comp_buff);
                rvk_cmd_draw(comp_buff);
                time = get_time();
                memcpy(ubo.mapped, &time, sizeof(float));
            end_mode_3d();
        end_drawing(); // ends rendering pass
    }

    rvk_wait_idle();
    rvk_buff_destroy(ubo);
    rvk_buff_destroy(comp_buff);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_destroy_descriptor_set_layout(compute_ds_layout.handle);
    rvk_destroy_pl_res(compute_pl, compute_pl_layout);
    rvk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    close_window();
    return 0;
}
