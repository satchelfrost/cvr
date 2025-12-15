#include "cvr.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "../nob.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h" // https://github.com/jkuhlmann/cgltf

typedef struct {
    uint16_t *items;
    size_t count;
    size_t capacity;
} glTF_Indices;

typedef struct {
    Vector3 position;
    Vector2 texcoord;
    Vector3 normal;
    Vector4 tanget;
} glTF_Vertex;

typedef struct {
    glTF_Vertex *items;
    size_t count;
    size_t capacity;
} glTF_Vertices;

typedef enum {
    MATERIAL_BASE               = (1 << 0),
    MATERIAL_METALLIC_ROUGHNESS = (1 << 1),
    MATERIAL_NORMAL             = (1 << 2),
} Material_Flags;

typedef struct {
    uint32_t flags;
    size_t base_image_index;
    size_t metallic_roughness_image_index;
    size_t normal_image_index;
	float base_color_factor[4];
	float metallic_factor;
	float roughness_factor;
} glTF_Material;

typedef enum {
    ATTRIBUTE_POSITION = (1 << 0),
    ATTRIBUTE_TEXCOORD = (1 << 1),
    ATTRIBUTE_NORMAL   = (1 << 2),
    ATTRIBUTE_TANGET   = (1 << 3),
} Attribute_Flags;

typedef struct {
    Rvk_Buffer vtx_buff;
    Rvk_Buffer idx_buff;
    glTF_Material material;
    glTF_Indices indices;
    glTF_Vertices vertices;
    Attribute_Flags flags;
} glTF_Primitive;

typedef struct {
    glTF_Primitive *items;
    size_t count;
    size_t capacity;
} glTF_Mesh;

typedef struct {
    glTF_Mesh *items;
    size_t count;
    size_t capacity;
} glTF_Meshes;

typedef struct {
    Rvk_Texture *items;
    size_t count;
    size_t capacity;
} glTF_Textures;

const char *cgltf_res_to_str(cgltf_result res)
{
    switch (res) {
    case cgltf_result_success:         return "success";
    case cgltf_result_data_too_short:  return "data_too_short";
    case cgltf_result_unknown_format:  return "unknown_format";
    case cgltf_result_invalid_json:    return "invalid_json";
    case cgltf_result_invalid_gltf:    return "invalid_gltf";
    case cgltf_result_invalid_options: return "invalid_options";
    case cgltf_result_file_not_found:  return "file_not_found";
    case cgltf_result_io_error:        return "io_error";
    case cgltf_result_out_of_memory:   return "out_of_memory";
    case cgltf_result_legacy_gltf:     return "legacy_gltf";
    default:
        assert(0 && "unreachable");
    }
}

const char *cgltf_attr_type_to_str(cgltf_attribute_type attr_type)
{
    switch (attr_type) {
    case cgltf_attribute_type_position: return "position";
    case cgltf_attribute_type_normal:   return "normal";
    case cgltf_attribute_type_tangent:  return "tangent";
    case cgltf_attribute_type_texcoord: return "texcoord";
    case cgltf_attribute_type_color:    return "color";
    case cgltf_attribute_type_joints:   return "joints";
    case cgltf_attribute_type_weights:  return "weights";
    case cgltf_attribute_type_custom:   return "custom";
    case cgltf_attribute_type_invalid:  return "invalid";
    default:
        assert(0 && "unreachable");
    }
}

void fill_attribute_groups(glTF_Primitive *primitive, cgltf_attribute attribute)
{
    Vector3 *positions = NULL;
    Vector3 *normals   = NULL;
    Vector2 *texcoords = NULL;
    Vector4 *tangets   = NULL;

    switch (attribute.type) {
    case cgltf_attribute_type_position:
        assert(attribute.data->type == cgltf_type_vec3);
        positions = (Vector3 *)cgltf_buffer_view_data(attribute.data->buffer_view);
        primitive->flags |= ATTRIBUTE_POSITION;
        for (size_t i = 0; i < attribute.data->count; i++) {
            if (primitive->vertices.count <= i) {
                glTF_Vertex vertex = {.position = positions[i]};
                da_append(&primitive->vertices, vertex);
            } else {
                primitive->vertices.items[i].position = positions[i];
            }
        }
        break;
    case cgltf_attribute_type_normal:
        assert(attribute.data->type == cgltf_type_vec3);
        normals = (Vector3 *)cgltf_buffer_view_data(attribute.data->buffer_view);
        primitive->flags |= ATTRIBUTE_NORMAL;
        for (size_t i = 0; i < attribute.data->count; i++) {
            if (primitive->vertices.count < i) {
                glTF_Vertex vertex = {.normal = normals[i]};
                da_append(&primitive->vertices, vertex);
            } else {
                primitive->vertices.items[i].normal = normals[i];
            }
        }
        break;
    case cgltf_attribute_type_tangent:
        assert(attribute.data->type == cgltf_type_vec4);
        tangets = (Vector4 *)cgltf_buffer_view_data(attribute.data->buffer_view);
        primitive->flags |= ATTRIBUTE_TANGET;
        for (size_t i = 0; i < attribute.data->count; i++) {
            if (primitive->vertices.count < i) {
                glTF_Vertex vertex = {.tanget = tangets[i]};
                da_append(&primitive->vertices, vertex);
            } else {
                primitive->vertices.items[i].tanget = tangets[i];
            }
        }
        break;
    case cgltf_attribute_type_texcoord:
        assert(attribute.data->type == cgltf_type_vec2);
        texcoords = (Vector2 *)cgltf_buffer_view_data(attribute.data->buffer_view);
        primitive->flags |= ATTRIBUTE_TEXCOORD;
        for (size_t i = 0; i < attribute.data->count; i++) {
            if (primitive->vertices.count < i) {
                glTF_Vertex vertex = {.texcoord = texcoords[i]};
                da_append(&primitive->vertices, vertex);
            } else {
                primitive->vertices.items[i].texcoord = texcoords[i];
            }
        }
        break;
    default:
        printf("attribute %s unsupported", cgltf_attr_type_to_str(attribute.type));
        assert(0);
    }
}

int main()
{
    /* initialize vulkan */
    init_window(500, 500, "sponza");

    /* read and parse the gltf file */
    const char *gltf_file_name = "res/Sponza.gltf";
    String_Builder sb = {0};
    if (!read_entire_file(gltf_file_name, &sb)) return 1;
    cgltf_options options = {0};
    cgltf_data *gltf_data = NULL;
    cgltf_result res = cgltf_parse(&options, sb.items, sb.count, &gltf_data);
    if (res != cgltf_result_success) printf("failed to parse %s: error %s\n", gltf_file_name, cgltf_res_to_str(res));
    res = cgltf_load_buffers(&options, gltf_data, gltf_file_name);
    if (res != cgltf_result_success) printf("failed to load buffers: error %s\n", cgltf_res_to_str(res));

    /* load meshes */
    glTF_Meshes meshes = {0};

    for (size_t m = 0; m < gltf_data->meshes_count; m++) {

        glTF_Mesh mesh = {0};

        for (size_t p = 0; p < gltf_data->meshes[m].primitives_count; p++) {
            cgltf_primitive primitive = gltf_data->meshes[m].primitives[p];
            assert(primitive.type == cgltf_primitive_type_triangles);

            /* interleave the attributes for this primitive */
            glTF_Primitive gltf_primitive = {0};
            for (size_t a = 0; a < primitive.attributes_count; a++)
                fill_attribute_groups(&gltf_primitive, primitive.attributes[a]);

            /* grab material indices */
            cgltf_texture *texture = NULL;
            texture = primitive.material->pbr_metallic_roughness.base_color_texture.texture;
            if (texture) {
                gltf_primitive.material.base_image_index = cgltf_image_index(gltf_data, texture->image);
                gltf_primitive.material.flags |= MATERIAL_BASE;
            }
            texture = primitive.material->normal_texture.texture;
            if (texture) {
                gltf_primitive.material.normal_image_index = cgltf_image_index(gltf_data, texture->image);
                gltf_primitive.material.flags |= MATERIAL_METALLIC_ROUGHNESS;
            }
            texture = primitive.material->pbr_metallic_roughness.metallic_roughness_texture.texture;
            if (texture) {
                gltf_primitive.material.metallic_roughness_image_index = cgltf_image_index(gltf_data, texture->image);
                gltf_primitive.material.flags |= MATERIAL_NORMAL;
            }

            /* grab indices */
            assert(primitive.indices->component_type == cgltf_component_type_r_16u);
            uint16_t *indices = (uint16_t *)cgltf_buffer_view_data(primitive.indices->buffer_view);
            for (size_t i = 0; i < primitive.indices->count; i++)
                da_append(&gltf_primitive.indices, indices[i]);

            /* upload buffers to GPU */
            size_t count = gltf_primitive.vertices.count;
            size_t size  = count * sizeof(*gltf_primitive.vertices.items);
            gltf_primitive.vtx_buff = rvk_create_vertex_buffer(size, count, gltf_primitive.vertices.items);
            count = gltf_primitive.indices.count;
            size  = count * sizeof(*gltf_primitive.indices.items);
            gltf_primitive.idx_buff = rvk_create_index_buffer(size, count, gltf_primitive.indices.items);

            da_append(&mesh, gltf_primitive);
        }
        da_append(&meshes, mesh);
    }

    /* load images */
    glTF_Textures textures = {0};
    for (size_t i = 0; i < gltf_data->images_count; i++) {
        const char *image_path = temp_sprintf("res/%s", gltf_data->images[i].uri);
        Rvk_Texture texture = load_texture_from_image(image_path);
        da_append(&textures, texture);
    }

    return 0;
}
