#include "cvr.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "../nob.h"

#define READ_ATTR(sv, type) (sv.count -= sizeof(type), sv.data += sizeof(type), (*(type*)(sv.data - sizeof(type))))

typedef struct {
    Vector3 position;
    Vector3 normal;
    Vector3 color;
} Vertex_Attribute;

typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout pl_layout;

    struct {
        uint32_t *items;
        size_t count;
        size_t capacity;
    } indices;

    struct {
        Vertex_Attribute *items;
        size_t count;
        size_t capacity;
    } vertices;

    Rvk_Buffer vtx_buff;
    Rvk_Buffer idx_buff;
} Ply;

typedef struct {
    float16 model;
} Push_Const;

void create_pipeline(Ply *ply)
{
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(Push_Const)
    };
    rvk_create_pipeline_layout(
        &ply->pl_layout,
        .p_push_constant_ranges = &pk_range,
    );

    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex_Attribute, position), },
        { .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex_Attribute, color), },
        { .location = 2, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex_Attribute, normal),   },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vertex_Attribute),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = RVK_ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };
    VkPipelineRasterizationStateCreateInfo rasterization_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
    };
    rvk_create_graphics_pipelines(&ply->pipeline,
                                  .vertex_shader_name   = "res/default.vert.glsl.spv",
                                  .fragment_shader_name = "res/default.frag.glsl.spv",
                                  .p_vertex_input_state = &vertex_input_ci,
                                  .p_rasterization_state = &rasterization_state_ci,
                                  .layout = ply->pl_layout);
}

bool load_ply(const char *file_path, Ply *ply)
{
    String_Builder sb = {0};
    if (!read_entire_file(file_path, &sb)) return false;
    String_View sv = sb_to_sv(sb);
    size_t vertex_count;
    size_t face_count;

    /* read header information */
    while (sv.count) {
        String_View lhs = sv_chop_by_delim(&sv, '\n');
        if (lhs.count && lhs.data[lhs.count - 1] == '\r')
            lhs.count--;
        const char *line = temp_sv_to_cstr(lhs);
        if (strstr(line, "element vertex")) {
            sv_chop_by_delim(&lhs, ' ');
            sv_chop_by_delim(&lhs, ' ');
            const char *vtx_count_str = temp_sv_to_cstr(lhs);
            vertex_count = atoi(vtx_count_str);
        }
        if (strstr(line, "element face")) {
            sv_chop_by_delim(&lhs, ' ');
            sv_chop_by_delim(&lhs, ' ');
            const char *face_count_str = temp_sv_to_cstr(lhs);
            face_count = atoi(face_count_str);
        }
        if (strstr(line, "end_header")) break;
    }

    for (size_t i = 0 ; i < vertex_count; i++) {
        double x = READ_ATTR(sv, double);
        double y = READ_ATTR(sv, double);
        double z = READ_ATTR(sv, double);
        double nx = READ_ATTR(sv, double);
        double ny = READ_ATTR(sv, double);
        double nz = READ_ATTR(sv, double);
        uint8_t r = READ_ATTR(sv, uint8_t);
        uint8_t g = READ_ATTR(sv, uint8_t);
        uint8_t b = READ_ATTR(sv, uint8_t);
        Vertex_Attribute attr = {
            .position = {x, y, z},
            .normal = {nx, ny, nz},
            .color = {r/255.0f, g/255.0f, b/255.0f},
        };
        da_append(&ply->vertices, attr);
    }

    for (size_t i = 0 ; i < face_count; i++) {
        uint8_t list_count = READ_ATTR(sv, uint8_t);
        assert(list_count == 3);
        uint32_t idx0 = READ_ATTR(sv, uint32_t);
        uint32_t idx1 = READ_ATTR(sv, uint32_t);
        uint32_t idx2 = READ_ATTR(sv, uint32_t);
        da_append(&ply->indices, idx0);
        da_append(&ply->indices, idx1);
        da_append(&ply->indices, idx2);
    }

    sb_free(sb);
    return true;
}

int main()
{
    Camera camera = {
        .position   = {0.0f, 1.0f, 2.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    Ply ply = {0};
    if (!load_ply("res/arena_full_d11_r0.05.ply", &ply)) return 1;

    init_window(1600, 900, "ply");

    size_t count = ply.vertices.count;
    size_t size = count * sizeof(*ply.vertices.items);
    ply.vtx_buff = rvk_create_vertex_buffer(size, count, ply.vertices.items);
    count = ply.indices.count;
    size = count * sizeof(*ply.indices.items);
    ply.idx_buff = rvk_create_index_buffer(size, count, ply.indices.items);
    create_pipeline(&ply);


    while(!window_should_close()) {
        if (is_key_down(KEY_F)) log_fps();
        update_camera_free(&camera);
        begin_drawing(BEIGE);
        begin_mode_3d(camera);
            rvk_cmd_bind_pipeline(ply.pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
            rag_standard_viewport_scissor();
            Matrix mvp = {0};
            get_mvp(&mvp);
            Push_Const pc = { .model = MatrixToFloatV(mvp), };
            VkShaderStageFlags flags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
            rvk_push_const(ply.pl_layout, flags, sizeof(Push_Const), &pc);
            rvk_draw_buffers_idx32(ply.vtx_buff, ply.idx_buff);
        end_mode_3d();
        end_drawing();
    }
    rvk_wait_idle();
    rvk_destroy_pl_res(ply.pipeline, ply.pl_layout);
    rvk_destroy_buffer(ply.vtx_buff);
    rvk_destroy_buffer(ply.idx_buff);

    close_window();

    return 0;
}
