#include "cvr.h"

#define CGLTF_IMPLEMENTATION
#include "ext/cgltf.h"

#define NOB_IMPLEMENTATION
#include "../../nob.h"

#define DEFAULT_ALBEDO MAGENTA

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
    const char *file_name;
    size_t mat_count;
    Color *albedos;
    Mesh *meshes;
    size_t mesh_count;
    size_t *mesh_albedo_idx;
} Model;

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;
VkDescriptorSetLayout ds_layout_ubo;
VkDescriptorSet ds_ubo;
VkDescriptorPool pool;

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

bool load_gltf(Model *model)
{
    bool result = true;
    Nob_String_Builder sb = {0};
    cgltf_data *data = NULL;
    cgltf_options options = {0};

    /* parse the gltf file */
    if (!nob_read_entire_file(model->file_name, &sb)) nob_return_defer(false);
    printf("file %s has %zu bytes\n", model->file_name, sb.count);
    cgltf_result res = cgltf_parse(&options, sb.items, sb.count, &data);
    if (res > cgltf_result_success) {
        printf("failed to parse %s; %s\n", model->file_name, cgltf_res_to_str(res));
        nob_return_defer(false);
    }
    if (data->file_type != cgltf_file_type_glb) {
        printf("for now only type '.glb' is recognized\n");
        nob_return_defer(false);
    }

    /* collect gltf info */
    printf("    mesh count: %zu\n", data->meshes_count);
    size_t primitive_count = 0;
    for (size_t i = 0; i < data->meshes_count; i++) {
        printf("        mesh: %zu, name: '%s', primitive count: %zu\n",
                i, data->meshes[i].name, data->meshes[i].primitives_count);
        primitive_count += data->meshes[i].primitives_count;
        for (size_t j = 0; j < data->meshes[i].primitives_count; j++) {
            printf("            primitive: %zu, type: %d\n", j, data->meshes[i].primitives[j].type);
        }
    }
    printf("        -------------------\n");
    printf("        primitive count: %zu\n", primitive_count);
    printf("    material count: %zu\n", data->materials_count);
    printf("    buffer count: %zu\n", data->buffers_count);
    printf("    image count: %zu\n", data->images_count);
    printf("    texture count: %zu\n", data->textures_count);

    if (data->images_count) {
        printf("currently don't support loading images yet");
        nob_return_defer(false);
    }

    /* reading buffers here fills buffer_view->buffer->data */
    res = cgltf_load_buffers(&options, data, model->file_name);
    if (res > cgltf_result_success) {
        printf("load buffers: %s\n", cgltf_res_to_str(res));
        nob_return_defer(false);
    }

    /* load albedo material */
    model->albedos = malloc(data->materials_count * sizeof(Color));
    model->mat_count = data->materials_count;
    bool has_base_mat = false;
    for (size_t i = 0; i < data->materials_count; i++) {
        model->albedos[i] = DEFAULT_ALBEDO;
        if (data->materials[i].has_pbr_metallic_roughness) {
            has_base_mat = true;
            Color albedo = {
                .r = data->materials[i].pbr_metallic_roughness.base_color_factor[0] * 255,
                .g = data->materials[i].pbr_metallic_roughness.base_color_factor[1] * 255,
                .b = data->materials[i].pbr_metallic_roughness.base_color_factor[2] * 255,
                .a = data->materials[i].pbr_metallic_roughness.base_color_factor[3] * 255,
            };
            model->albedos[i] = albedo;
            printf("    material %zu albedo: (%i, %i, %i, %i)\n", i, albedo.r, albedo.g, albedo.b, albedo.a);
        }
    }
    if (!has_base_mat) {
        printf("no base material found, until we have a default, this is required\n");
        nob_return_defer(false);
    }

    /* load mesh data */
    model->mesh_count = primitive_count;
    model->meshes = calloc(primitive_count, sizeof(Mesh));
    model->mesh_albedo_idx = calloc(primitive_count, sizeof(size_t));
    for (size_t i = 0, mesh_idx = 0; i < data->meshes_count; i++) {
        for (size_t j = 0; j < data->meshes[i].primitives_count; j++) {
            /* only support triangles for now */
            if (data->meshes[i].primitives[j].type != cgltf_primitive_type_triangles) continue;

            /* load attributes */
            for (size_t k = 0; k < data->meshes[i].primitives[j].attributes_count; k++) {
                cgltf_attribute_type attr_type = data->meshes[i].primitives[j].attributes[k].type;
                const char *attr_type_str = cgltf_attr_type_to_str(attr_type);
                cgltf_accessor *attr = data->meshes[i].primitives[j].attributes[k].data;
                switch (attr_type) {
                case cgltf_attribute_type_position:
                    if (attr->component_type != cgltf_component_type_r_32f && attr->type != cgltf_type_vec3) {
                        printf("position must have: component type r_32f, and vec3");
                        nob_return_defer(false);
                    } else {
                        model->meshes[mesh_idx].vtx_count = attr->count;
                        model->meshes[mesh_idx].vertices = malloc(attr->count * 3 * sizeof(float));
                        LOAD_ATTR(attr, 3, float, model->meshes[mesh_idx].vertices);
                    }
                    break;
                case cgltf_attribute_type_normal:
                    if (attr->component_type != cgltf_component_type_r_32f && attr->type != cgltf_type_vec3) {
                        printf("normal must have: component type r_32f, and vec3");
                        nob_return_defer(false);
                    } else {
                        model->meshes[mesh_idx].normals = malloc(attr->count * 3 * sizeof(float));
                        LOAD_ATTR(attr, 3, float, model->meshes[mesh_idx].normals);
                    }
                    break;
                case cgltf_attribute_type_tangent:
                case cgltf_attribute_type_texcoord:
                case cgltf_attribute_type_color:
                case cgltf_attribute_type_joints:
                case cgltf_attribute_type_weights:
                case cgltf_attribute_type_custom:
                case cgltf_attribute_type_invalid:
                default:
                    // printf("attribute type: '%s' not supported, attr count: %zu\n", attr_type_str, attr->count);
                    (void)attr_type_str;
                }
            }

            /* load indices */
            if (data->meshes[i].primitives[j].indices != NULL) {
                cgltf_accessor *attr = data->meshes[i].primitives[j].indices;
                model->meshes[mesh_idx].tri_count = attr->count / 3;
                if (attr->component_type != cgltf_component_type_r_16u) {
                    printf("received component type %d, must be r16u\n", attr->component_type);
                    nob_return_defer(false);
                }
                model->meshes[mesh_idx].indices = malloc(attr->count * sizeof(unsigned short));
                LOAD_ATTR(attr, 1, unsigned short, model->meshes[mesh_idx].indices);
            }

            /* assign materials to the*/
            for (size_t m = 0; m < data->materials_count; m++) {
                if (&data->materials[m] == data->meshes[i].primitives[j].material) {
                    model->mesh_albedo_idx[mesh_idx] = m;
                    break;
                }
            }

            mesh_idx++;
        }
    }

defer:
    nob_sb_free(sb);
    cgltf_free(data);
    return result;
}

void unload_model(Model *model)
{
    free(model->albedos);
    free(model->mesh_albedo_idx);
    for (size_t i = 0; i < model->mesh_count; i++) {
        free(model->meshes[i].vertices);
        free(model->meshes[i].texcoords);
        free(model->meshes[i].normals);
        free(model->meshes[i].colors);
        free(model->meshes[i].indices);
    }
    free(model->meshes);
}

bool create_pipeline()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(float16),
    };
    VkPush
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!vk_pl_layout_init(layout_ci, &gfx_pl_layout)) return false;

    /* create pipeline */ 
    VkVertexInputAttributeDescription vert_attrs[] = {
        {.format = VK_FORMAT_R32G32B32_SFLOAT},
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vector3),
    };
    Pipeline_Config config = {
        .pl_layout = gfx_pl_layout,
        .vert = "./res/gltf.vert.spv",
        .frag = "./res/gltf.frag.spv",
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .vert_attrs = vert_attrs,
        .vert_attr_count = VK_ARRAY_LEN(vert_attrs),
        .vert_bindings = &vert_bindings,
        .vert_binding_count = 1,
    };
    if (!vk_basic_pl_init(config, &gfx_pl)) return false;

    return true;
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

    Model robot = {.file_name = "res/robot.glb"};
    if (!load_gltf(&robot)) return 1;

    // if (!create_pipeline()) return 1;

    return 0;

    if (!init_window(800, 600, "gltf")) return 1;
    while (!window_should_close()) {
        begin_drawing(GREEN);
        begin_mode_3d(camera);
            if (!draw_shape(SHAPE_CUBE)) return 1;
        end_mode_3d();
        end_drawing();
    }

    unload_model(&robot);
    close_window();
    return 0;
}
