#include "cvr.h"

#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../nob.h"

typedef struct {
    Vector2 pos;
    Vector3 color;
    Vector2 uv;
} Quad_Vertex;

typedef struct {
    VkPipeline handle;
    VkPipelineLayout layout;
} Pipeline;

typedef enum {
    MOVEMENT_LEFT  = 1 << 0,
    MOVEMENT_RIGHT = 1 << 1,
    MOVEMENT_DOWN  = 1 << 2,
    MOVEMENT_UP    = 1 << 3,
} Movement;

typedef enum {
    MODE_TILE,
    MODE_TEXTURE,
    MODE_COUNT,
} Mode;

typedef struct {
    int movement;
    int index;
    int tiles_per_side;
    int mode;
    int reset;
    float speed;
} UBO_Data;

typedef struct {
    UBO_Data data;
    Rvk_Buffer buff;
} UBO;

#define OFFSET_COUNT 25
typedef struct {
    Vector2 xy_offsets[OFFSET_COUNT];
    Rvk_Buffer buff;
} SSBO;

typedef struct {
    Quad_Vertex *items;
    size_t count;
    size_t capacity;
} Quad_Vertices;

typedef struct {
    uint16_t *items;
    size_t count;
    size_t capacity;
} Quad_Indices;

typedef struct {
    Quad_Vertices vertices;
    Quad_Indices indices;
    int tiles_per_side;
    Rvk_Buffer vtx_buff;
    Rvk_Buffer idx_buff;
} Procedural_Mesh;

Procedural_Mesh gen_procedural_mesh(size_t tiles_per_side)
{
/*
 *      start_idx = row*verts_per_row + col 
 *
 *      A = (start_idx    , start_idx + 1                , start_idx + verts_per_row)
 *      B = (start_idx + 1, start_idx + verts_per_row + 1, start_idx + verts_per_row)
 *
 *         0        1        2        3
 *     +--------+--------+--------+--------+
 *     |       /|       /|       /|       /|
 *     |  A   / |  A   / |  A   / |  A   / |
 *     |     /  |     /  |     /  |     /  |
 *  0  |    /   |    /   |    /   |    /   |
 *     |   /    |   /    |   /    |   /    |
 *     |  /     |  /     |  /     |  /     |
 *     | /   B  | /   B  | /   B  | /   B  |
 *     |/       |/       |/       |/       |
 *     +--------+--------+--------+--------+
 *     |       /|       /|       /|       /|
 *     |  A   / |  A   / |  A   / |  A   / |
 *     |     /  |     /  |     /  |     /  |
 *  1  |    /   |    /   |    /   |    /   |
 *     |   /    |   /    |   /    |   /    |
 *     |  /     |  /     |  /     |  /     |
 *     | /   B  | /   B  | /   B  | /   B  |
 *     |/       |/       |/       |/       |
 *     +--------+--------+--------+--------+
 * */

    Procedural_Mesh mesh = {0};

    if (tiles_per_side == 0) tiles_per_side = 1;
    // if (tiles_per_side > 4) tiles_per_side = 4;
    mesh.tiles_per_side = tiles_per_side;
    size_t verts_per_side = tiles_per_side + 1;

    float dy = -0.5;
    for (size_t y = 0; y < verts_per_side; y++) {
        float dx = -0.5;
        for (size_t x = 0; x < verts_per_side; x++) {
            // append vertices
            Quad_Vertex vert = {.pos = {dx, dy}, .uv = {dx+0.5f, dy+0.5f}};
            da_append(&mesh.vertices, vert);
            dx += 1.0f/tiles_per_side;

        }
        dy += 1.0f/tiles_per_side;
    }

    for (size_t y = 0; y < tiles_per_side; y++) {
        for (size_t x = 0; x < tiles_per_side; x++) {
            // append indices
            uint16_t start_idx = y*verts_per_side + x;
            // A
            da_append(&mesh.indices, start_idx);
            da_append(&mesh.indices, start_idx+1);
            da_append(&mesh.indices, start_idx+verts_per_side);
            // B
            da_append(&mesh.indices, start_idx+1);
            da_append(&mesh.indices, start_idx+verts_per_side+1);
            da_append(&mesh.indices, start_idx+verts_per_side);
        }
    }

    return mesh;
}

void create_pipeline(Pipeline *pl, VkDescriptorSetLayout *ds_layout)
{
    rvk_create_pipeline_layout(&pl->layout, .p_set_layouts=ds_layout);
    VkVertexInputAttributeDescription vert_attrs[] = {
        { .location = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Quad_Vertex, pos)},
        { .location = 1, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Quad_Vertex, uv) },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Quad_Vertex),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = RVK_ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };
    rvk_create_graphics_pipelines(
        &pl->handle,
        .vertex_shader_name   = "res/texture_distort.vert.glsl.spv",
        .fragment_shader_name = "res/texture_distort.frag.glsl.spv",
        .p_vertex_input_state = &vertex_input_ci,
        .layout = pl->layout,
    );
}

void print_vertex_info(Procedural_Mesh mesh)
{
    for (size_t i = 0; i < mesh.vertices.count; i++) {
        Quad_Vertex v = mesh.vertices.items[i];
        printf("v(%zu); pos = {%f, %f}, uv = {%f, %f}\n", i, v.pos.x, v.pos.y, v.uv.x, v.uv.y);
    }
}

#define DEAD_ZONE 0.25
bool moved_in_direction(Movement m)
{
    float joy_x = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_X);
    float joy_y = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_Y);
    switch (m) {
    case MOVEMENT_LEFT:  return (is_key_down(KEY_LEFT)  || joy_x < -DEAD_ZONE);
    case MOVEMENT_RIGHT: return (is_key_down(KEY_RIGHT) || joy_x >  DEAD_ZONE);
    case MOVEMENT_DOWN:  return (is_key_down(KEY_DOWN)  || joy_y >  DEAD_ZONE);
    case MOVEMENT_UP:    return (is_key_down(KEY_UP)    || joy_y < -DEAD_ZONE);
    }
    return false;
}

// TODO: pass sb by pointer, and boolean the header print
bool write_config_file(Rvk_Buffer buff, int tile_size)
{
    bool result = true;
    String_Builder sb = {0};
    
    sb_append_cstr(&sb, "CAVE config file format by Reese Gallagher\n");
    sb_append_cstr(&sb, "there should be four of the following blocks\n");
    sb_append_cstr(&sb, "for each of the four walls (front, left, right, floor)\n");
    sb_append_cstr(&sb, "+----------------+\n");
    sb_append_cstr(&sb, "| block          |\n");
    sb_append_cstr(&sb, "+----------------+\n");
    sb_append_cstr(&sb, "| wall name      |\n");
    sb_append_cstr(&sb, "| vertex count   |\n");
    sb_append_cstr(&sb, "| vertex offsets |\n");
    sb_append_cstr(&sb, "+----------------+\n");
    sb_append_cstr(&sb, "end_header\n");
    sb_append_cstr(&sb, "front\n");
    size_t vertex_count = (tile_size+1)*(tile_size+1);
    sb_append_cstr(&sb, temp_sprintf("%zu\n", vertex_count));
    Vector2 *offsets = (Vector2 *)buff.mapped;
    for (size_t i = 0; i < vertex_count; i++)
        sb_append_cstr(&sb, temp_sprintf("%f,%f\n", offsets[i].x, offsets[i].y));

    const char *file_name = temp_sprintf("cave_config_%dx%d.txt", tile_size, tile_size);
    result = write_entire_file(file_name, sb.items, sb.count);
    if (result) printf("successfully saved %s\n", file_name);
    else printf("failed to write %s", file_name);

    sb_free(sb);
    return result;
}

Rvk_Buffer create_hos_vis_buff_from_existing(Rvk_Buffer existing)
{
    Rvk_Buffer host_vis = {0};
    rvk_buff_init(
        existing.size,
        existing.count,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,    // so we can transer to this buffer
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, // so we can map
        RVK_BUFFER_TYPE_COMPUTE,
        existing.data,
        &host_vis
    );
    rvk_buff_copy(host_vis, existing, 0);
    rvk_buff_map(&host_vis);
    // should we do a memcopy from the mapped to data?
    return host_vis;
}

#define MESH_COUNT 4

typedef struct {
    String_View *items;
    size_t count;
    size_t capacity;
} String_Views;

String_Views sv_splitlines(String_View sv)
{
    String_Views svs = {0};

    while (sv.count) {
        String_View lhs = sv_chop_by_delim(&sv, '\n');

        // if carriage return, eliminate
        if (lhs.count && lhs.data[lhs.count - 1] == '\r')
            lhs.count--;

        da_append(&svs, lhs);
    }

    return svs;
}

typedef enum {
    PARSE_STATE_HEADER,
    PARSE_STATE_WALL_NAME,
    PARSE_STATE_VTX_COUNT,
    PARSE_STATE_READ_VTX,
} Parse_State;

bool parse_config(const char *file_name, SSBO *ssbo)
{
    String_Builder sb = {0};
    if (!nob_read_entire_file(file_name, &sb))
        return false;

    String_View sv = sb_to_sv(sb);
    String_Views svs = sv_splitlines(sv);

    Parse_State parse_state = PARSE_STATE_HEADER;
    int vertex_count = 0;
    int vertex_id = 0;
    for (size_t i = 0; i < svs.count; i++) {
        String_View sv = svs.items[i];
        const char *line = temp_sv_to_cstr(sv);
        switch (parse_state) {
        case PARSE_STATE_HEADER:
            if (strcmp(line, "end_header") == 0)
                parse_state = PARSE_STATE_WALL_NAME;
            break;
        case PARSE_STATE_WALL_NAME:
            parse_state = PARSE_STATE_VTX_COUNT;
            printf("wall name: %s\n", line);
            break;
        case PARSE_STATE_VTX_COUNT:
            vertex_count = atoi(line);
            printf("vertex count %d\n", vertex_count);
            parse_state = PARSE_STATE_READ_VTX;
            break;
        case PARSE_STATE_READ_VTX:
            if (vertex_id < vertex_count) {
                assert(vertex_count <= OFFSET_COUNT);
                String_View lhs = sv_chop_by_delim(&sv, ',');
                float x = strtof(temp_sv_to_cstr(lhs), NULL);
                float y = strtof(temp_sv_to_cstr(sv), NULL);
                ssbo->xy_offsets[vertex_id] = (Vector2){x, y};
                if (++vertex_id == vertex_count)
                    parse_state = PARSE_STATE_WALL_NAME;
            }
            break;
        }
    }

    sb_free(sb);
    return true;
}

int main(int argc, char **argv)
{
    shift(argv, argc); // program
    char *cfg_file = NULL;
    if (argc) cfg_file = shift(argv, argc);

    init_window(1000, 1000, "distortion mapping");

    // Rvk_Texture tex = load_texture_from_image("res/shrek_face.png");
    // Rvk_Texture tex = load_texture_from_image("res/checker-map_tho.png");
    Rvk_Texture tex = load_texture_from_image("res/checker.png");

    size_t mesh_idx = 0;
    Procedural_Mesh meshes[MESH_COUNT] = {0};
    for (size_t i = 0; i < MESH_COUNT; i++) {
        meshes[i] = gen_procedural_mesh(i+1);
        rvk_upload_vtx_buff(
            meshes[i].vertices.count*sizeof(Quad_Vertex),
            meshes[i].vertices.count,
            meshes[i].vertices.items,
            &meshes[i].vtx_buff
        );
        rvk_upload_idx_buff(
            meshes[i].indices.count*sizeof(uint16_t),
            meshes[i].indices.count,
            meshes[i].indices.items,
            &meshes[i].idx_buff
        );
    }

    UBO ubo = {0};
    rvk_uniform_buff_init(sizeof(UBO_Data), &ubo.data, &ubo.buff);
    rvk_buff_map(&ubo.buff);
    ubo.data.tiles_per_side = meshes[mesh_idx].tiles_per_side;
    memcpy(ubo.buff.mapped, &ubo.data, sizeof(UBO_Data));

    SSBO ssbo = {0};
    if (cfg_file) {
        if (!parse_config(cfg_file, &ssbo)) return 1;
        else printf("successfully loaded config file %s\n", cfg_file);
    }
    rvk_comp_buff_init(OFFSET_COUNT*sizeof(Vector2), OFFSET_COUNT, ssbo.xy_offsets, &ssbo.buff);
    rvk_buff_staged_upload(ssbo.buff);
    Rvk_Buffer copied_buff = {0};

    VkDescriptorSet ds;
    Rvk_Descriptor_Set_Layout ds_layout = {0};
    Rvk_Descriptor_Pool_Arena arena = {0};
    rvk_descriptor_pool_arena_init(&arena);
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    rvk_ds_layout_init(bindings, RVK_ARRAY_LEN(bindings), &ds_layout);
    rvk_descriptor_pool_arena_alloc_set(&arena, &ds_layout, &ds);
    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &ubo.buff.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &ssbo.buff.info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &tex.info,
        },
    };
    rvk_update_ds(RVK_ARRAY_LEN(writes), writes);

    Pipeline pl = {0};
    create_pipeline(&pl, &ds_layout.handle);

    set_target_fps(120);
    while (!window_should_close()) {
        float dt = get_frame_time();

        // handle input
        if (is_key_pressed(KEY_T) && !is_key_down(KEY_LEFT_SHIFT)) {
            mesh_idx = (mesh_idx + 1) % MESH_COUNT;
            ubo.data.tiles_per_side = mesh_idx + 1;
            ubo.data.index = 0;
        }
        if (is_key_pressed(KEY_T) && is_key_down(KEY_LEFT_SHIFT)) {
            mesh_idx = (mesh_idx - 1 + MESH_COUNT) % MESH_COUNT;
            ubo.data.tiles_per_side = mesh_idx + 1;
        }
        float joy_x = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_X);
        float joy_y = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_Y);
        Vector2 v = {joy_x, joy_y};
        float l = Vector2Length(v);
        if (l >= DEAD_ZONE) {
            ubo.data.speed = dt*l/2.0f;
        } else {
            ubo.data.speed = is_key_down(KEY_LEFT_SHIFT) ? 0.1f*dt : 0.5f*dt;
        }

        ubo.data.movement = moved_in_direction(MOVEMENT_LEFT)  ? ubo.data.movement | (1<<0) : ubo.data.movement & ~(1<<0);
        ubo.data.movement = moved_in_direction(MOVEMENT_RIGHT) ? ubo.data.movement | (1<<1) : ubo.data.movement & ~(1<<1);
        ubo.data.movement = moved_in_direction(MOVEMENT_DOWN)  ? ubo.data.movement | (1<<2) : ubo.data.movement & ~(1<<2);
        ubo.data.movement = moved_in_direction(MOVEMENT_UP)    ? ubo.data.movement | (1<<3) : ubo.data.movement & ~(1<<3);
        if (is_key_pressed(KEY_M) || is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            ubo.data.mode = (ubo.data.mode + 1) % MODE_COUNT;
        }
        ubo.data.reset = (is_key_down(KEY_R)) ? 1 : 0;

        if ((is_key_pressed(KEY_SPACE) && !is_key_down(KEY_LEFT_SHIFT)) ||
            is_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
            ubo.data.index = (ubo.data.index + 1) % meshes[mesh_idx].vertices.count;
        if ((is_key_pressed(KEY_SPACE) && is_key_down(KEY_LEFT_SHIFT)) ||
            is_gamepad_button_pressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1))
            ubo.data.index = (ubo.data.index - 1 + meshes[mesh_idx].vertices.count) % meshes[mesh_idx].vertices.count;

        if (is_key_pressed(KEY_L)) {
            rvk_wait_idle();
            if (!copied_buff.handle)
                copied_buff = create_hos_vis_buff_from_existing(ssbo.buff);
            Vector2 *offsets = (Vector2 *)copied_buff.mapped;
            for (size_t i = 0; i < meshes[mesh_idx].vertices.count; i++) {
                printf("x = %f, y = %f\n", offsets[i].x, offsets[i].y);
            }
        }

        if (is_key_pressed(KEY_W)) {
            if (copied_buff.mapped) {
                if (!write_config_file(copied_buff, mesh_idx + 1)) return 1;
            }
        }

        // handle drawing
        begin_drawing(BLACK);
            rvk_bind_gfx(pl.handle, pl.layout, &ds, 1);
            rvk_draw_buffers(meshes[mesh_idx].vtx_buff, meshes[mesh_idx].idx_buff);
            memcpy(ubo.buff.mapped, &ubo.data, sizeof(UBO_Data));
        end_drawing();
    }

    rvk_wait_idle();
    for (size_t i = 0; i < MESH_COUNT; i++) {
        rvk_buff_destroy(meshes[i].vtx_buff);
        rvk_buff_destroy(meshes[i].idx_buff);
    }
    rvk_buff_destroy(ubo.buff);
    rvk_buff_destroy(ssbo.buff);
    rvk_buff_destroy(copied_buff);
    rvk_unload_texture(tex);
    rvk_destroy_descriptor_set_layout(ds_layout.handle);
    rvk_descriptor_pool_arena_destroy(arena);
    rvk_destroy_pl_res(pl.handle, pl.layout);
    close_window();
    return 0;
}
