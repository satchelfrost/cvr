#include "cvr.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "../nob.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h" // https://github.com/jkuhlmann/cgltf

typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} glTF_Indices;

typedef struct {
    Vector3 position;
    Vector2 uv;
    Vector3 normal;
    Vector4 tanget;
} glTF_Vertex;

typedef struct {
    glTF_Vertex *items;
    size_t count;
    size_t capacity;
    Rvk_Buffer buff;
} glTF_Vertices;

typedef struct {
    glTF_Vertices vertices;
    int material_id; 
} glTF_Mesh;

typedef struct {
    glTF_Mesh *items;
    size_t count;
    size_t capacity;
} glTF_Model;

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

	printf("meshes_count %zu\n",    gltf_data->meshes_count);
	printf("materials_count %zu\n", gltf_data->materials_count);
	printf("accessors_count %zu\n", gltf_data->accessors_count);
	printf("buffers count %zu\n", gltf_data->buffers_count);
	printf("buffer views count %zu\n", gltf_data->buffer_views_count);
	printf("samplers count %zu\n", gltf_data->samplers_count);
	printf("skins count %zu\n", gltf_data->skins_count);
	printf("lights count %zu\n", gltf_data->skins_count);

    return 0;
}
