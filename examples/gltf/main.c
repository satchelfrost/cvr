#include "cvr.h"

#define CGLTF_IMPLEMENTATION
#include "ext/cgltf.h"

#define NOB_IMPLEMENTATION
#include "../../nob.h"

typedef struct {
    size_t vtx_count;
    size_t tri_count;
    float *vertices;
    float *texcoords;
    float *normals;
    unsigned char *colors;
    unsigned short *indices;
} Mesh;

typedef struct {
    const char *res_file_name;
    size_t mat_count;
    Color *albedo;
    Mesh *meshes;
    size_t mesh_count;
} Model;

#define LOAD_ATTR(accessor, comp_count, data_type, dst_ptr)                     \
    do {                                                                        \
        int n = 0;                                                              \
        data_type *buffer = accessor->buffer_view->buffer->data +               \
                            accessor->buffer_view->offset / sizeof(data_type) + \
                            accessor->offset / sizeof(data_type);               \
        for (unsigned int k = 0; k < accessor->count; k++) {                    \
            for (int l = 0; l < comp_count; l++)                                \
                dst_ptr[comp_count * k + l] = buffer[n + l];                    \
            n += accessor->stride / sizeof(data_type);                          \
        }                                                                       \
    } while (0)

const char *cgltf_res_to_str(cgltf_result res)
{
    switch (res) {
    case cgltf_result_success:         return "cgltf_result_success";
    case cgltf_result_data_too_short:  return "cgltf_result_data_too_short";
    case cgltf_result_unknown_format:  return "cgltf_result_unknown_format";
    case cgltf_result_invalid_json:    return "cgltf_result_invalid_json";
    case cgltf_result_invalid_gltf:    return "cgltf_result_invalid_gltf";
    case cgltf_result_invalid_options: return "cgltf_result_invalid_options";
    case cgltf_result_file_not_found:  return "cgltf_result_file_not_found";
    case cgltf_result_io_error:        return "cgltf_result_io_error";
    case cgltf_result_out_of_memory:   return "cgltf_result_out_of_memory";
    case cgltf_result_legacy_gltf:     return "cgltf_result_legacy_gltf";
    default:
        assert(0 && "unreachable");
    }
}

bool load_gltf(Model *model)
{
    bool result = true;
    Nob_String_Builder sb = {0};
    cgltf_data *data = NULL;
    cgltf_options options = {0};

    /* parse the gltf file */
    if (!nob_read_entire_file(model->res_file_name, &sb)) nob_return_defer(false);
    printf("file %s has %zu bytes\n", model->res_file_name, sb.count);
    cgltf_result res = cgltf_parse(&options, sb.items, sb.count, &data);
    if (res > cgltf_result_success) {
        printf("failed to parse %s with error code %s\n", model->res_file_name, cgltf_res_to_str(res));
        nob_return_defer(false);
    }
    if (data->file_type != cgltf_file_type_glb) {
        printf("for now only type '.glb' is recognized\n");
        nob_return_defer(false);
    }

    /* collect gltf info */
    printf("    mesh count %zu\n", data->meshes_count);
    size_t primitive_count = 0; // TODO: do something with this...
    for (size_t i = 0; i < data->meshes_count; i++) {
        printf("        meshes[%s].primitives_count %zu\n", data->meshes[i].name, data->meshes[i].primitives_count);
        primitive_count += data->meshes[i].primitives_count;
        for (size_t j = 0; j < data->meshes[i].primitives_count; j++) {
            printf("            primitive type %d\n", data->meshes[i].primitives[j].type);
        }
    }
    printf("        -------------------\n");
    printf("        primitive count %zu\n", primitive_count);
    printf("    material count %zu\n", data->materials_count);
    printf("    buffer count %zu\n", data->buffers_count);
    printf("    image count %zu\n", data->images_count);
    printf("    texture count %zu\n", data->textures_count);

    if (data->images_count) {
        printf("currently don't support loading images yet");
        nob_return_defer(false);
    }

    // TODO: may want to call cgltf_load_buffers() here, which apparently filles buffer_view->buffer->data

    /* load albedo materials */
    model->albedo = calloc(data->materials_count, sizeof(Color));
    model->mat_count = data->materials_count;
    bool has_base_mat = false;
    for (size_t i = 0; i < data->materials_count; i++) {
        if (data->materials[i].has_pbr_metallic_roughness) {
            has_base_mat = true;
            Color albedo = {
                .r = data->materials[i].pbr_metallic_roughness.base_color_factor[0] * 255,
                .g = data->materials[i].pbr_metallic_roughness.base_color_factor[1] * 255,
                .b = data->materials[i].pbr_metallic_roughness.base_color_factor[2] * 255,
                .a = data->materials[i].pbr_metallic_roughness.base_color_factor[3] * 255,
            };
            model->albedo[i] = albedo;
        }
    }
    if (!has_base_mat) {
        printf("no base material found, until we have a default, this is required\n");
        nob_return_defer(false);
    }

    /* load mesh data */
    model->mesh_count = primitive_count;
    model->meshes = calloc(primitive_count, sizeof(Mesh));


defer:
    nob_sb_free(sb);
    cgltf_free(data);
    return result;
}

void unload_model(Model *model)
{
    free(model->albedo);
    for (size_t i = 0; i < model->mesh_count; i++) {
        free(model->meshes[i].vertices);
        free(model->meshes[i].texcoords);
        free(model->meshes[i].normals);
        free(model->meshes[i].colors);
        free(model->meshes[i].indices);
    }
    free(model->meshes);
}

int main()
{
    Camera camera = {
        .position   = {5.0f, 5.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45,
        .projection = PERSPECTIVE,
    };

    Model robot = {.res_file_name = "res/robot.glb"};
    return load_gltf(&robot) ? 0 : 1;

    if (!init_window(800, 600, "gltf")) return 1;
    while (!window_should_close()) {
        begin_drawing(GREEN);
        begin_mode_3d(camera);
            if (!draw_shape(SHAPE_CUBE)) return 1;
        end_mode_3d();
        end_drawing();
    }
    close_window();
    return 0;
}
