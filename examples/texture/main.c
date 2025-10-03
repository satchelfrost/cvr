#include "cvr.h"

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

typedef struct {
    int width, height;
    void *data;
} Image;

typedef struct {
    Vector3 pos;
    Vector3 color;
    Vector2 tex_coord;
} Texture_Vertex; 

Image load_image(const char *file_name)
{
    Image img = {0};
    int channels;
    img.data = stbi_load(file_name, &img.width, &img.height, &channels, STBI_rgb_alpha);

    if (!img.data) {
        rvk_log(RVK_ERROR, "image %s could not be loaded", file_name);
    } else {
        rvk_log(RVK_INFO, "image %s was successfully loaded", file_name);
        rvk_log(RVK_INFO, "    (height, width) = (%d, %d)", img.height, img.width);
    }

    return img;
}

// TODO: could this be promoted to an API function?
void update_texture(Rvk_Texture tex, VkDescriptorSet ds)
{
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &tex.info,
    };
    rvk_update_ds(1, &write);
}

bool create_pipeline(VkDescriptorSetLayout *ds_layout)
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(float16)};
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = ds_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!rvk_pl_layout_init(layout_ci, &gfx_pl_layout)) return false;

    /* create pipeline */
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Texture_Vertex, pos),
        },
        {
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Texture_Vertex, color),
        },
        {
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Texture_Vertex, tex_coord),
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Texture_Vertex),
    };
    Pipeline_Config config = {
        .pl_layout = gfx_pl_layout,
        .vert = "./res/texture.vert.glsl.spv",
        .frag = "./res/texture.frag.glsl.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = RVK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!rvk_basic_pl_init(config, &gfx_pl)) return false;

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

    Image matrix = load_image("res/matrix.png");
    Image statue = load_image("res/statue.jpg");
    Rvk_Texture matrix_tex = {0};
    Rvk_Texture statue_tex = {0};

    if (!rvk_load_texture(matrix.data, matrix.width, matrix.height, VK_FORMAT_R8G8B8A8_SRGB, &matrix_tex)) {
        rvk_log(RVK_ERROR, "matrix texture failed to upload");
        return 1;
    }
    if (!rvk_load_texture(statue.data, statue.width, statue.height, VK_FORMAT_R8G8B8A8_SRGB, &statue_tex)) {
        rvk_log(RVK_ERROR, "statue texture failed to upload");
        return 1;
    }

    /* setup descriptors */
    VkDescriptorSet matrix_ds, statue_ds;
    Rvk_Descriptor_Set_Layout texture_layout = {0};
    Rvk_Descriptor_Pool_Arena arena = {0};
    if (!rvk_descriptor_pool_arena_init(&arena)) return 1;

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    if (!rvk_ds_layout_init(&binding, 1, &texture_layout)) return 1;

    rvk_log_descriptor_pool_usage(arena);
    if (!rvk_descriptor_pool_arena_alloc_set(&arena, &texture_layout, &matrix_ds)) return 1;
    if (!rvk_descriptor_pool_arena_alloc_set(&arena, &texture_layout, &statue_ds)) return 1;
    rvk_log_descriptor_pool_usage(arena);
    update_texture(matrix_tex, matrix_ds);
    update_texture(statue_tex, statue_ds);
    if (!create_pipeline(&texture_layout.handle)) return 1;

    float time = 0.0f;
    while(!window_should_close()) {
        time = get_time();
        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(time);
            push_matrix();
                scale(matrix.width/(float)matrix.height, 1.0f, 1.0f);
                if (!draw_shape_ex(gfx_pl, gfx_pl_layout, matrix_ds, SHAPE_QUAD))
                    return 1;
            pop_matrix();

            scale(statue.width/(float)statue.height, 1.0f, 1.0f);
            translate(0.0f, 0.0f, 1.0f);
                if (!draw_shape_ex(gfx_pl, gfx_pl_layout, statue_ds, SHAPE_QUAD))
                    return 1;
            rotate_x(time);
            draw_shape_wireframe(SHAPE_CUBE);
        end_mode_3d();
        end_drawing();
    }

    wait_idle();
    rvk_destroy_descriptor_set_layout(texture_layout.handle);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_unload_texture(matrix_tex);
    rvk_unload_texture(statue_tex);
    rvk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    close_window();
    return 0;
}
