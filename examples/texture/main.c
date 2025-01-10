#include "cvr.h"
#include "ext/nob.h"
#include "ext/raylib-5.0/raymath.h"
#include <math.h>
#include "vk_ctx.h"

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

typedef enum {
    DS_MATRIX,
    DS_STATUE,
    DS_COUNT,
} Ds_Name;

VkDescriptorSet ds_sets[DS_COUNT];
VkDescriptorSetLayout ds_layout;
VkDescriptorPool ds_pool;
VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

typedef struct {
    const char *file_name;
    Vk_Texture handle;
    float aspect;
} Texture;

#define TEXTURE_COUNT 2
Texture textures[TEXTURE_COUNT] = {
    {.file_name = "res/matrix.png"},
    {.file_name = "res/statue.jpg"}
};

bool upload_texture(Texture *texture)
{
    bool result = true;
    int channels, width, height;
    void *data = stbi_load(texture->file_name, &width, &height, &channels, STBI_rgb_alpha);

    if (!data) {
        nob_log(NOB_ERROR, "image %s could not be loaded", texture->file_name);
        nob_return_defer(false);
    } else {
        nob_log(NOB_INFO, "image %s was successfully loaded", texture->file_name);
        nob_log(NOB_INFO, "    (height, width) = (%d, %d)", height, width);
        nob_log(NOB_INFO, "    image size in memory = %d bytes", height * width * 4);
    }

    if (!vk_load_texture(data, width, height, VK_FORMAT_R8G8B8A8_SRGB, &texture->handle))
        nob_return_defer(false);

    texture->aspect = (float)width / height;

defer:
    free(data);
    return result;
}

bool setup_ds_layout()
{
    VkDescriptorSetLayoutBinding binding = {DS_BINDING(0, COMBINED_IMAGE_SAMPLER, FRAGMENT_BIT)};
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pBindings = &binding,
        .bindingCount = 1,
    };
    if (!vk_create_ds_layout(layout_ci, &ds_layout)) return false;
    return true;
}

bool setup_ds_pool()
{
    VkDescriptorPoolSize pool_size = {DS_POOL(COMBINED_IMAGE_SAMPLER, DS_COUNT)};
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = DS_COUNT,
    };
    if (!vk_create_ds_pool(pool_ci, &ds_pool)) return false;
    return true;
}

bool setup_ds_sets()
{
    /* allocate descriptor sets based on layouts */
    VkDescriptorSetLayout layouts[] = {ds_layout, ds_layout};
    VkDescriptorSetAllocateInfo alloc = {DS_ALLOC(layouts, NOB_ARRAY_LEN(layouts), ds_pool)};
    if (!vk_alloc_ds(alloc, ds_sets)) return false;

    /* update descriptor sets based on layouts */
    for (size_t i = 0; i < TEXTURE_COUNT; i++) {
        VkDescriptorImageInfo img_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView   = textures[i].handle.view,
            .sampler     = textures[i].handle.sampler,
        };
        VkWriteDescriptorSet write = {
            DS_WRITE_IMG(0, COMBINED_IMAGE_SAMPLER, ds_sets[i], &img_info)
        };
        vk_update_ds(1, &write);
    }

    return true;
}

bool create_pipeline()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float16)};
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ds_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!vk_pl_layout_init(layout_ci, &gfx_pl_layout)) return false;

    /* create pipeline */
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, pos),
        },
        {
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color),
        },
        {
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, tex_coord),
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vertex),
    };
    Pipeline_Config config = {
        .pl_layout = gfx_pl_layout,
        .vert = "./res/texture.vert.spv",
        .frag = "./res/texture.frag.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = NOB_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!vk_basic_pl_init(config, &gfx_pl)) return false;

    return true;
}

int main()
{
    Camera camera = {
        .position   = {0.0f, 1.0f, 4.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(500, 500, "Load texture");

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
        if (!upload_texture(&textures[i])) return 1;

    /* setup descriptors */
    if (!setup_ds_layout()) return 1;
    if (!setup_ds_pool())   return 1;
    if (!setup_ds_sets())   return 1;

    if (!create_pipeline()) return 1;

    float time = 0.0f;
    while(!window_should_close()) {
        time = get_time();
        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(time);
            push_matrix();
                scale(textures[DS_MATRIX].aspect, 1.0f, 1.0f);
                if (!draw_shape_ex(gfx_pl, gfx_pl_layout, ds_sets[DS_MATRIX], SHAPE_QUAD))
                    return 1;
            pop_matrix();

            scale(textures[DS_STATUE].aspect, 1.0f, 1.0f);
            translate(0.0f, 0.0f, 1.0f);
                if (!draw_shape_ex(gfx_pl, gfx_pl_layout, ds_sets[DS_STATUE], SHAPE_QUAD))
                    return 1;
            rotate_x(time);
            draw_shape_wireframe(SHAPE_CUBE);
        end_mode_3d();
        end_drawing();
    }

    wait_idle();
    vk_unload_texture(&textures[DS_MATRIX].handle);
    vk_unload_texture(&textures[DS_STATUE].handle);
    vk_destroy_ds_layout(ds_layout);
    vk_destroy_ds_pool(ds_pool);
    vk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    close_window();
    return 0;
}
