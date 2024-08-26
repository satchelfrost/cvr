#include "cvr.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>
#include "ext/nob.h"

#define PARTICLE_COUNT 256
//#define PARTICLE_COUNT 8192

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    Vector4 color;
} Particle;

Particle particles[PARTICLE_COUNT];
VkDescriptorSet compute_ds;
VkDescriptorSetLayout compute_ds_layout;
VkPipeline compute_pl;
VkPipelineLayout compute_pl_layout;
Vk_Buffer ssbo;

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

bool setup_ds_layout()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        {DS_BINDING(0, UNIFORM_BUFFER, COMPUTE_BIT)},
        {DS_BINDING(1, STORAGE_BUFFER, COMPUTE_BIT)},
    };

    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = NOB_ARRAY_LEN(bindings),
        .pBindings = bindings,
    };

    return vk_create_ds_layout(layout_ci, &render_ds_layout);
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {DS_POOL(UNIFORM_BUFFER, 1)},
        {DS_POOL(STORAGE_BUFFER, 1)},
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = NOB_ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = 1,
    };

    return vk_create_ds_pool(pool_ci, &pool);
}

void setup_ds_sets()
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(&compute_ds_layout, 1, pool)};
    if (!vk_alloc_ds(alloc, &compute_ds)) return false;

    /* update descriptor sets */
    VkDescriptorBufferInfo ubo_info = {
        .buffer = ubo.buff.handle,
        .range = ubo.buff.size,
    };
    VkDescriptorBufferInfo ssbo_info = {
        .buffer = ssbo.handle,
        .range = ssbo.size,
    };
    VkWriteDescriptorSet writes[] = {
        {DS_WRITE_BUFF(0, UNIFORM_BUFFER, compute_ds,  &ubo_info)},
        {DS_WRITE_BUFF(1, STORAGE_BUFFER, compute_ds,  &ssbo_info)},
    };

    vk_update_ds(NOB_ARRAY_LEN(writes), writes);
}

int main()
{
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    float width  = 800.0f;
    float height = 600.0f;
    init_window(width, height, "Compute Shader");
    set_target_fps(60);

    /* initialize the particles */
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
        particle->color = color_to_vec4(colors[i % NOB_ARRAY_LEN(colors)]);
    }

    Compute_Buffer ssbo = {
        .buff = {
            .items = particles,
            .size  = PARTICLE_COUNT * sizeof(Particle),
            .count = PARTICLE_COUNT,
        },
    };

    // if (!upload_compute_points(ssbo.buff, &ssbo.id, EXAMPLE_COMPUTE)) return 1;
    // if (!ssbo_init(EXAMPLE_COMPUTE)) return 1;

    /* setup descriptors */
    if (!setup_ds_layout()) return 1;
    if (!setup_ds_pool())   return 1;
    if (!setup_ds())        return 1;

    float time = 0.0f;
    Buffer buff = {
        .size  = sizeof(float),
        .count = 1,
        .items = &time,
    };
    if (!ubo_init(buff, EXAMPLE_COMPUTE)) return 1;

    // if (!vk_pl_layout_init(render_ds_layout, &render_pl_layout))                        return 1;
    // if (!vk_compute_pl_init2("./res/render.comp.spv", render_pl_layout, &render_pl))    return 1;
    //
    // if (!vk_rec_compute()) return 1;
    //     size_t group_x = ceil(ssbos.items[0].count / 256);
    //     vk_compute2(render_pl, render_pl_layout, ds_sets[DS_RENDER], group_x, 1, 1);
    //     vk_compute_pl_barrier();
    //     vk_compute2(resolve_pl, resolve_pl_layout, ds_sets[DS_RESOLVE], 2048 / 16, 2048 / 16, 1);
    // if (!vk_end_rec_compute()) return 1;

    while (!window_should_close()) {
        time = get_time();
        begin_compute();
            if (!compute(EXAMPLE_COMPUTE)) return 1;
        end_compute();

        begin_drawing(BLACK);
        begin_mode_3d(camera);
            if (!draw_points(ssbo.id, EXAMPLE_COMPUTE)) return 1;
        end_mode_3d();
        end_drawing();
    }

    destroy_compute_buff(ssbo.id, EXAMPLE_COMPUTE);
    close_window();
    return 0;
}
