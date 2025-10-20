#include "cvr.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../../nob.h"

#define PIXEL_WIDTH 400
#define PIXEL_HEIGHT PIXEL_WIDTH
#define FIRST_CHAR 32
#define CHAR_COUNT 96 

typedef struct {
    Vector2 ndc;
    Vector2 uv;
} Quad_2D_Vertex;

#define NDC_UPPER_LEFT  {-1.0f, -1.0f}
#define NDC_UPPER_RIGHT { 1.0f, -1.0f}
#define NDC_LOWER_LEFT  {-1.0f,  1.0f}
#define NDC_LOWER_RIGHT { 1.0f,  1.0f}
#define UV_UPPER_LEFT  {0.0f, 0.0f}
#define UV_UPPER_RIGHT {1.0f, 0.0f}
#define UV_LOWER_LEFT  {0.0f, 1.0f}
#define UV_LOWER_RIGHT {1.0f, 1.0f}

Quad_2D_Vertex quad_2d_vertices[] = {
    {.ndc = NDC_UPPER_LEFT,  .uv = UV_UPPER_LEFT }, // 0
    {.ndc = NDC_UPPER_RIGHT, .uv = UV_UPPER_RIGHT}, // 1
    {.ndc = NDC_LOWER_LEFT,  .uv = UV_LOWER_LEFT }, // 2
    {.ndc = NDC_LOWER_RIGHT, .uv = UV_LOWER_RIGHT}, // 3
};

uint16_t quad_2d_indices[] = {
    0, 1, 2, 2, 1, 3
};

Rvk_Descriptor_Pool_Arena arena = {0};

typedef struct {
    VkPipeline pl;
    VkPipelineLayout pl_layout;
    Rvk_Descriptor_Set_Layout ds_layout;
    VkDescriptorSet ds;
} Pipeline;

Pipeline quad_2d = {0};

void setup_ds_layout()
{
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    rvk_ds_layout_init(&binding, 1, &quad_2d.ds_layout);
}

void update_ds(Rvk_Texture bitmap_texture)
{
    rvk_descriptor_pool_arena_alloc_set(&arena, &quad_2d.ds_layout, &quad_2d.ds);
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &bitmap_texture.info,
        .dstSet = quad_2d.ds,
    };
    rvk_update_ds(1, &write);
}

typedef struct {
    int x;
    int y;
    int x0;
    int y0;
    int x1;
    int y1;
    float xoffset;
    float yoffset;
} My_Push_Const;

void create_pipeline()
{
    VkPushConstantRange pk_range = {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .size = sizeof(My_Push_Const)};
    rvk_create_pipeline_layout(&quad_2d.pl_layout, .p_set_layouts = &quad_2d.ds_layout.handle, .p_push_constant_ranges = &pk_range);
    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Quad_2D_Vertex, ndc), },
        { .location = 1, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Quad_2D_Vertex, uv),  },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Quad_2D_Vertex),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = RVK_ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
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
    rvk_create_graphics_pipelines(&quad_2d.pl,
                                  .vertex_shader_name   = "res/text.vert.glsl.spv",
                                  .fragment_shader_name = "res/text.frag.glsl.spv",
                                  .p_vertex_input_state  = &vertex_input_ci,
                                  .p_color_blend_state = &color_blend_ci,
                                  .layout = quad_2d.pl_layout);
}

void draw_quad_2d(Rvk_Buffer vtx_buff, Rvk_Buffer idx_buff, stbtt_bakedchar *cdata)
{
    rvk_bind_gfx(quad_2d.pl, quad_2d.pl_layout, &quad_2d.ds, 1);

    const char *hello = "Hello World!";
    int x = 0;
    int y = 0;
    for (size_t i = 0; i < strlen(hello); i++) {
        stbtt_bakedchar cd = cdata[hello[i]-32];
        My_Push_Const pk = {
            .x = x + cd.xoff,
            .y = y + 63 + cd.yoff,
            .x0 = cd.x0,
            .y0 = cd.y0,
            .x1 = cd.x1,
            .y1 = cd.y1,
            .xoffset = 3,
            .yoffset = -35,
        };
        rvk_push_const(quad_2d.pl_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(My_Push_Const), &pk);
        rvk_draw_buffers(vtx_buff, idx_buff);
        x += cd.xadvance-1;
    }
}

void print_cdata(int i, stbtt_bakedchar *cdata)
{
    stbtt_bakedchar f = cdata[i];
    printf("cdata[%i] x0 = %u, y0 = %u, x1 = %u, y1 = %u, xoff = %f, yoff = %f, xadvance = %f\n",
           i, f.x0, f.y0, f.x1, f.y1, f.xoff, f.yoff, f.xadvance);
}

int main()
{
    /* load and bake text */
    String_Builder sb = {0};
    const char *file = "res/RobotoMono-Medium.ttf";
    if (!read_entire_file(file, &sb)) return 1;
    printf("byte count %zu\n", sb.count);
    float pixel_height = 64.0f;
    unsigned char bitmap[PIXEL_WIDTH*PIXEL_HEIGHT];
    stbtt_bakedchar cdata[CHAR_COUNT]; // ASCII 32..126 is 95 glyphs
    stbtt_BakeFontBitmap((unsigned char *)sb.items, 0,
                         pixel_height, bitmap,
                         PIXEL_WIDTH, PIXEL_HEIGHT,
                         FIRST_CHAR,
                         CHAR_COUNT,
                         cdata);

    init_window(400, 400, "text");

    // const char *hello = "Reese";
    // for (size_t i = 0; i < strlen(hello); i++) {
    //     print_cdata(hello[i]-32, cdata);
    // }
    for (size_t i = 0; i < CHAR_COUNT; i++) {
        print_cdata(i, cdata);
    }
    print_cdata('R'-32, cdata);
    print_cdata('e'-32, cdata);


    // upload the quad
    Rvk_Buffer vtx_buff = rvk_create_vertex_buffer(4*sizeof(Quad_2D_Vertex), 4, quad_2d_vertices);
    Rvk_Buffer idx_buff = rvk_create_index_buffer(6*sizeof(uint16_t), 6, quad_2d_indices);
    // upload the texture
    Rvk_Texture bitmap_texture = rvk_load_texture(bitmap, PIXEL_WIDTH, PIXEL_HEIGHT, VK_FORMAT_R8_UNORM);

    arena = rvk_create_descriptor_pool_arena();
    setup_ds_layout();
    update_ds(bitmap_texture);
    create_pipeline();

    set_target_fps(60);
    while(!window_should_close()) {
        begin_drawing(BLUE);
            draw_quad_2d(vtx_buff, idx_buff, cdata);
        end_drawing();
    }

    rvk_wait_idle();
    rvk_destroy_buffer(vtx_buff);
    rvk_destroy_buffer(idx_buff);
    rvk_destroy_texture(bitmap_texture);
    rvk_destroy_descriptor_set_layout(quad_2d.ds_layout.handle);
    rvk_destroy_descriptor_pool_arena(arena);
    rvk_destroy_pl_res(quad_2d.pl, quad_2d.pl_layout);
    close_window();
    return 0;
}
