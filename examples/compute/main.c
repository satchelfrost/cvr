#include "cvr.h"
#include "geometry.h"

#define PARTICLE_COUNT 800

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    Vector4 color;
} Particle;

static Particle particles[PARTICLE_COUNT];

/* Vulkan state */
VkDescriptorSet compute_ds;
VkDescriptorSetLayout compute_ds_layout;
VkPipelineLayout compute_pl_layout;
VkPipeline compute_pl;
VkDescriptorPool pool;
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
        particle->color = color_to_vec4(colors[i % VK_ARRAY_LEN(colors)]);
    }
}

bool setup_ds_layout()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = VK_ARRAY_LEN(bindings),
        .pBindings = bindings,
    };
    return vk_create_ds_layout(layout_ci, &compute_ds_layout);
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 1)},
        {DS_POOL(STORAGE_BUFFER, 1)},
    };
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = VK_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 1,
    };
    return vk_create_ds_pool(pool_ci, &pool);
}

bool setup_ds(Vk_Buffer ubo, Vk_Buffer comp_buff)
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(&compute_ds_layout, 1, pool)};
    if (!vk_alloc_ds(alloc, &compute_ds)) return false;

    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = ubo.handle,
        .range = ubo.size,
    };
    VkDescriptorBufferInfo comp_info = {
        .buffer = comp_buff.handle,
        .range = comp_buff.size,
    };
    VkWriteDescriptorSet writes[] = {
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, compute_ds,  &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, compute_ds,  &comp_info)},
    };
    vk_update_ds(VK_ARRAY_LEN(writes), writes);

    return true;
}

bool create_pipeline()
{
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &compute_ds_layout,
    };
    if (!vk_pl_layout_init(layout_ci, &compute_pl_layout))                             return false;
    if (!vk_compute_pl_init("./res/default.comp.glsl.spv", compute_pl_layout, &compute_pl)) return false;

    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float16)};
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges = &pk_range;
    if (!vk_pl_layout_init(layout_ci, &gfx_pl_layout)) return false;
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32_SFLOAT, // position
        },
        {
            .location = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT, // color
            .offset = 16,
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vertex),
    };
    Pipeline_Config config = {
        .pl_layout = gfx_pl_layout,
        .vert = "./res/particle.vert.spv",
        .frag = "./res/particle.frag.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        .polygon_mode = VK_POLYGON_MODE_POINT,
        .vert_attrs = vert_attrs,
        .vert_attr_count = VK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!vk_basic_pl_init(config, &gfx_pl)) return false;

    return true;
}

int main()
{
    /* initialize buffers */
    Vk_Buffer comp_buff = {
        .size  = PARTICLE_COUNT * sizeof(Particle),
        .count = PARTICLE_COUNT,
    };
    float width  = 800.0f;
    float height = 600.0f;
    init_particles(particles, width, height);
    Vk_Buffer ubo = {.size = sizeof(float), .count = 1};

    /* init window and Vulkan */
    init_window(width, height, "Compute Shader");
    set_target_fps(60);
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    if (!vk_comp_buff_staged_upload(&comp_buff, particles)) return 1;
    if (!vk_ubo_init(&ubo)) return 1;

    /* setup descriptors */
    if (!setup_ds_layout())        return 1;
    if (!setup_ds_pool())          return 1;
    if (!setup_ds(ubo, comp_buff)) return 1;

    /* setup pipelines */
    if (!create_pipeline()) return 1;

    /* record compute commands */
    if (!vk_rec_compute()) return 1;
        /* +1 in case PARTICLE_COUNT is not a multiple of 256 */
        size_t group_x = PARTICLE_COUNT / 256 + 1;
        vk_compute(compute_pl, compute_pl_layout, compute_ds, group_x, 1, 1);
    if (!vk_end_rec_compute()) return 1;

    float time = 0.0f;
    while (!window_should_close()) {
        vk_compute_fence_wait();
        if (!vk_submit_compute()) return 1;

        begin_drawing(BLACK);
        begin_mode_3d(camera);
            Matrix mvp = {0};
            if (!get_mvp(&mvp)) return 1;
            float16 f16_mvp = MatrixToFloatV(mvp);
            vk_draw_points(comp_buff, &f16_mvp, gfx_pl, gfx_pl_layout, NULL, 0);
            time = get_time();
            memcpy(ubo.mapped, &time, sizeof(float));
        end_mode_3d();
        end_drawing();
    }

    wait_idle();
    vk_buff_destroy(&ubo);
    vk_buff_destroy(&comp_buff);
    vk_destroy_ds_pool(pool);
    vk_destroy_ds_layout(compute_ds_layout);
    vk_destroy_pl_res(compute_pl, compute_pl_layout);
    vk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    close_window();
    return 0;
}
