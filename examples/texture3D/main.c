#include "cvr.h"

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;

typedef struct {
    Vector3 pos;
    Vector3 color;
    Vector2 tex_coord;
} Texture_Vertex; 


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

void create_pipeline(VkDescriptorSetLayout *ds_layout)
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
    rvk_pl_layout_init(layout_ci, &gfx_pl_layout);

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
    rvk_basic_pl_init(config, &gfx_pl);
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

    Rvk_Texture matrix = load_texture_from_image("res/matrix.png");
    Rvk_Texture statue = load_texture_from_image("res/statue.jpg");

    /* setup descriptors */
    VkDescriptorSet matrix_ds, statue_ds;
    Rvk_Descriptor_Set_Layout texture_layout = {0};
    Rvk_Descriptor_Pool_Arena arena = {0};
    rvk_descriptor_pool_arena_init(&arena);

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    rvk_ds_layout_init(&binding, 1, &texture_layout);

    if (!rvk_descriptor_pool_arena_alloc_set(&arena, &texture_layout, &matrix_ds)) return 1;
    if (!rvk_descriptor_pool_arena_alloc_set(&arena, &texture_layout, &statue_ds)) return 1;
    update_texture(matrix, matrix_ds);
    update_texture(statue, statue_ds);
    create_pipeline(&texture_layout.handle);

    float time = 0.0f;
    while(!window_should_close()) {
        time = get_time();
        begin_drawing(BLACK);
        begin_mode_3d(camera);
            rotate_y(time);
            push_matrix();
                scale(matrix.img.extent.width/(float)matrix.img.extent.height, 1.0f, 1.0f);
                draw_shape_ex(gfx_pl, gfx_pl_layout, matrix_ds, SHAPE_QUAD);
            pop_matrix();

            scale(statue.img.extent.width/(float)statue.img.extent.height, 1.0f, 1.0f);
            translate(0.0f, 0.0f, 1.0f);
                draw_shape_ex(gfx_pl, gfx_pl_layout, statue_ds, SHAPE_QUAD);
            rotate_x(time);
            draw_shape_wireframe(SHAPE_CUBE);
        end_mode_3d();
        end_drawing();
    }

    rvk_wait_idle();
    rvk_destroy_descriptor_set_layout(texture_layout.handle);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_unload_texture(matrix);
    rvk_unload_texture(statue);
    rvk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    close_window();
    return 0;
}
