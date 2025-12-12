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
} glTF_Attribute_Group;

typedef struct {
    glTF_Attribute_Group *items;
    size_t count;
    size_t capacity;
    Rvk_Buffer buff;
} glTF_Attribute_Groups;

typedef struct {
    glTF_Attribute_Groups attr_groups;
    glTF_Indices indices;
} glTF_Mesh;

typedef struct {
    glTF_Mesh *items;
    size_t count;
    size_t capacity;
} glTF_Meshes;

typedef struct {
    const char *uri;
    String_Builder sb;
} glTF_Buffer;

typedef struct {
    glTF_Buffer *items;
    size_t count;
    size_t capacity;
} glTF_Buffers;

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

// float *load_attrribute_array()
// {
//
// }

int main()
{
    const char *gltf_file_name = "res/Sponza.gltf";
    String_Builder sb = {0};
    if (!read_entire_file(gltf_file_name, &sb)) return 1;
    printf("read in file %s\n", gltf_file_name);
    printf("    bytes: %zu\n", sb.count);

    cgltf_options options = {0};
    cgltf_data *gltf_data = NULL;
    cgltf_result res = cgltf_parse(&options, sb.items, sb.count, &gltf_data);
    if (res != cgltf_result_success) printf("failed to parse %s: error %s\n", gltf_file_name, cgltf_res_to_str(res));

    /* load buffers */
    glTF_Buffers buffers = {0};
    for (size_t i = 0; i < gltf_data->buffers_count; i++) {
        glTF_Buffer buffer = {0};
        buffer.uri = strdup(gltf_data->buffers[i].uri);
        const char *buffer_path = temp_sprintf("res/%s", buffer.uri);
        if (!read_entire_file(buffer_path, &buffer.sb)) return 1;
        printf("loaded buffer %s, %zu bytes\n", buffer_path, buffer.sb.count);
        da_append(&buffers, buffer);
    }

    /* load images */

    return 0;

    /* load meshes */
    glTF_Meshes meshes = {0};

    for (size_t m = 0; m < gltf_data->meshes_count; m++) {

        glTF_Mesh mesh = {0};

        for (size_t p = 0; p < gltf_data->meshes[m].primitives_count; p++) {
            cgltf_primitive primitive = gltf_data->meshes[m].primitives[p];
            assert(primitive.type == cgltf_primitive_type_triangles);

            /* get the attributes */
            // cgltf_buffer_view *position_view = NULL;
            // cgltf_buffer_view *normal_view   = NULL;
            // cgltf_buffer_view *texcoord_view = NULL;
            // cgltf_buffer_view *tanget_view   = NULL;

            glTF_Attribute_Group group = {0};
            // for (size_t a = 0; a < primitive.attributes_count; a++) {
            //     cgltf_attribute_type type = primitive.attributes[a].type;
            //     switch (type) {
            //     case cgltf_attribute_type_position:
            //         assert(primitive.attributes[a].data->type == cgltf_type_vec3);
            //         position_view = primitive.attributes[a].data->buffer_view;
            //         break;
            //     case cgltf_attribute_type_normal:
            //         assert(primitive.attributes[a].data->type == cgltf_type_vec3);
            //         normal_view = primitive.attributes[a].data->buffer_view;
            //         break;
            //     case cgltf_attribute_type_tangent:
            //         assert(primitive.attributes[a].data->type == cgltf_type_vec4);
            //         tanget_view = primitive.attributes[a].data->buffer_view;
            //         break;
            //     case cgltf_attribute_type_texcoord:
            //         assert(primitive.attributes[a].data->type == cgltf_type_vec2);
            //         texcorrd_view = primitive.attributes[a].data->buffer_view;
            //         break;
            //     default:
            //         printf("attribute %s unsupported", cgltf_attr_type_to_str(type));
            //         assert(0);
            //     }
            // }
            da_append(&mesh.attr_groups, group);
        }
        da_append(&meshes, mesh);
    }

    return 0;
}
