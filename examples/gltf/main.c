/*
 * This example renders a simple gltf. Please note:
 * this is not a generic gltf loader/renderer. It's
 * highly specific to the asset used; which, can
 * be found here:
 *
 * https://github.com/raysan5/raylib/blob/master/examples/models/resources/models/gltf/robot.glb
 *
 * Also, animations aren't currently working :/
 * */

#include "cvr.h"

#define CGLTF_IMPLEMENTATION
#include "ext/cgltf.h" // https://github.com/jkuhlmann/cgltf

#define NOB_IMPLEMENTATION
#include "../../nob.h"

#define DEFAULT_ALBEDO (Vector4){1.0f, 0.0f, 1.0f, 0.0f}

#define GLTF_ATTR_PTR(accessor_ptr, out_type) \
    (out_type *)(accessor_ptr)->buffer_view->buffer->data + \
    (accessor_ptr)->buffer_view->offset / sizeof(out_type) + \
    (accessor_ptr)->offset / sizeof(out_type)

typedef struct {
    Vector3 pos;
    Vector3 normal;
} Gltf_Vertex; 

typedef struct {
    Gltf_Vertex *vertices;
    unsigned short *indices;
    Vk_Buffer vtx_buff;
    Vk_Buffer idx_buff;
    unsigned char *bone_ids;
    float *bone_weights;
} Mesh;

typedef struct {
    size_t *parent_idxs;
    Matrix *matrices;
    Vector3 *translations;
    Quaternion *rotations;
    Vector3 *scales;
    size_t count;
} Bones;

typedef struct {
    const char *file_name;
    Vector4 *albedos;
    Mesh *meshes;
    size_t mesh_count;
    size_t *mesh_albedo_idx;
    Bones bones;
} Model;

typedef struct {
    float16 mvp;
    Vector4 color;
} Push_Const;

VkPipeline gfx_pl;
VkPipelineLayout gfx_pl_layout;
VkDescriptorSetLayout ds_layout_ubo;
VkDescriptorSet ds_ubo;
VkDescriptorPool pool;

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

bool load_attrs(cgltf_primitive primitive, Mesh *mesh_out)
{
    for (size_t i = 0; i < primitive.attributes_count; i++) {
        const char *attr_name = cgltf_attr_type_to_str(primitive.attributes[i].type);
        cgltf_accessor *attr = primitive.attributes[i].data;

        switch (primitive.attributes[i].type) {
        case cgltf_attribute_type_position:
            if (attr->component_type != cgltf_component_type_r_32f && attr->type != cgltf_type_vec3) {
                printf("position must have: component type r_32f, and attr type vec3");
                return false;
            } else {
                if (!mesh_out->vertices) {
                    size_t size = attr->count * sizeof(Gltf_Vertex);
                    mesh_out->vertices = malloc(size);
                    mesh_out->vtx_buff.count = attr->count;
                    mesh_out->vtx_buff.size  = size;
                }
                float *verts = GLTF_ATTR_PTR(attr, float);
                for (size_t a = 0; a < attr->count; a++) {
                    Vector3 pos = {
                        .x = verts[3 * a + 0],
                        .y = verts[3 * a + 1],
                        .z = verts[3 * a + 2],
                    };
                    mesh_out->vertices[a].pos = pos;
                }
            } break;
        case cgltf_attribute_type_normal:
            if (attr->component_type != cgltf_component_type_r_32f && attr->type != cgltf_type_vec3) {
                printf("normal must have: component type r_32f, and attr type vec3");
                return false;
            } else {
                if (!mesh_out->vertices) {
                    size_t size = attr->count * sizeof(Gltf_Vertex);
                    mesh_out->vertices = malloc(size);
                    mesh_out->vtx_buff.count = attr->count;
                    mesh_out->vtx_buff.size  = size;
                }
                float *normals = GLTF_ATTR_PTR(attr, float);
                for (size_t a = 0; a < attr->count; a++) {
                    Vector3 normal = {
                        .x = normals[3 * a + 0],
                        .y = normals[3 * a + 1],
                        .z = normals[3 * a + 2],
                    };
                    mesh_out->vertices[a].normal = normal;
                }
            } break;
        case cgltf_attribute_type_joints:
            if (attr->component_type != cgltf_component_type_r_8u && attr->type != cgltf_type_vec4) {
                printf("joint must have: component type r_8u, and attr type vec4");
                return false;
            } else {
                unsigned char *bone_ids = GLTF_ATTR_PTR(attr, unsigned char);
                size_t size = mesh_out->vtx_buff.count * 4 * sizeof(unsigned char);
                mesh_out->bone_ids = malloc(size);
                memcpy(mesh_out->bone_ids, bone_ids, size);
            } break;
        case cgltf_attribute_type_weights:
            if (attr->component_type != cgltf_component_type_r_32f && attr->type != cgltf_type_vec4) {
                printf("weight must have: component type r_32f, and attr type vec4");
                return false;
            } else {
                float *weights = GLTF_ATTR_PTR(attr, float);
                size_t size = mesh_out->vtx_buff.count * 4 * sizeof(float);
                mesh_out->bone_weights = malloc(size);
                memcpy(mesh_out->bone_weights, weights, size);
            } break;
        case cgltf_attribute_type_tangent:
        case cgltf_attribute_type_texcoord:
        case cgltf_attribute_type_color:
        case cgltf_attribute_type_custom:
        case cgltf_attribute_type_invalid:
        default:
#ifdef LOG_UNHANDLED_ATTR
            printf("attribute type: '%s' not supported\n", attr_name);
#else
            (void)attr_name;
#endif
        }
    }

    return true;
}

bool load_indices(cgltf_primitive primitive, Mesh *mesh_out)
{
    cgltf_accessor *attr = NULL;
    if ((attr = primitive.indices)) {
        float *indices = GLTF_ATTR_PTR(attr, float);
        if (attr->component_type != cgltf_component_type_r_16u) {
            printf("received component type %d, must be r16u\n", attr->component_type);
            return false;
        }
        size_t size = attr->count * sizeof(unsigned short);
        mesh_out->indices = malloc(size);
        mesh_out->idx_buff.size = size;
        mesh_out->idx_buff.count = attr->count;
        memcpy(mesh_out->indices, indices, size);
    }

    return true;
}

void load_bones(cgltf_skin skin, Bones *bones)
{
    bones->count = skin.joints_count;
    bones->parent_idxs  = malloc(skin.joints_count * sizeof(*bones->parent_idxs));
    bones->matrices     = malloc(skin.joints_count * sizeof(Matrix));
    bones->scales       = malloc(skin.joints_count * sizeof(Vector3));
    bones->translations = malloc(skin.joints_count * sizeof(Vector3));
    bones->rotations    = malloc(skin.joints_count * sizeof(Quaternion));

    for (size_t i = 0; i < skin.joints_count; i++) {
        cgltf_node node = *skin.joints[i];

        /* load the parent information */
        bones->parent_idxs[i] = -1;
        for (size_t j = 0; j < skin.joints_count; j++) {
            if (skin.joints[i] == node.parent) {
                bones->parent_idxs[i] = j;
                break;
            }
        }

        /* load each nodes local matrix */
        memcpy(&bones->matrices[i], node.matrix, sizeof(Matrix));
        memcpy(&bones->scales[i], node.scale, sizeof(Vector3));
        memcpy(&bones->rotations[i], node.rotation, sizeof(Quaternion));
        memcpy(&bones->translations[i], node.translation, sizeof(Vector3));
    }
}

void hierarchy_concat(Bones *bones)
{
    for (size_t i = 0; i < bones->count; i++) {
        if (bones->parent_idxs[i] == (size_t)-1) {
            if (bones->parent_idxs[i] > i) continue;
            bones->matrices[i] = MatrixMultiply(bones->matrices[i], bones->matrices[bones->parent_idxs[i]]);
            bones->rotations[i] = QuaternionMultiply(bones->rotations[bones->parent_idxs[i]], bones->rotations[i]);
            bones->translations[i] = Vector3RotateByQuaternion(bones->translations[i], bones->rotations[bones->parent_idxs[i]]);
            bones->translations[i] = Vector3Add(bones->translations[i], bones->translations[bones->parent_idxs[i]]);
            bones->scales[i] = Vector3Multiply(bones->scales[i], bones->scales[bones->parent_idxs[i]]);
        }
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
        printf("currently don't support loading images yet\n");
        nob_return_defer(false);
    }

    /* reading buffers here fills buffer_view->buffer->data */
    res = cgltf_load_buffers(&options, data, model->file_name);
    if (res > cgltf_result_success) {
        printf("load buffers: %s\n", cgltf_res_to_str(res));
        nob_return_defer(false);
    }

    /* load albedo material */
    model->albedos = malloc(data->materials_count * sizeof(Vector4));
    bool has_base_mat = false;
    for (size_t i = 0; i < data->materials_count; i++) {
        model->albedos[i] = DEFAULT_ALBEDO;
        if (data->materials[i].has_pbr_metallic_roughness) {
            has_base_mat = true;
            Vector4 color = {
                .x = data->materials[i].pbr_metallic_roughness.base_color_factor[0],
                .y = data->materials[i].pbr_metallic_roughness.base_color_factor[1],
                .z = data->materials[i].pbr_metallic_roughness.base_color_factor[2],
                .w = data->materials[i].pbr_metallic_roughness.base_color_factor[3],
            };
            model->albedos[i] = color;
            printf("    material %zu albedo: (%0.2f, %0.2f, %0.2f, %0.2f)\n", i, color.x, color.y, color.z, color.w);
        }
    }
    if (!has_base_mat) {
        printf("no base material found, until we have a default, this is required\n");
        nob_return_defer(false);
    }

    /* load bones if we have exactly one skin */
    if (data->skins_count == 1) {
        cgltf_skin skin = data->skins[0];
        load_bones(skin, &model->bones);
        hierarchy_concat(&model->bones);
    } else if (data->skins_count > 1) {
        printf("skin count greater than one not being handled\n");
    }

    /* load mesh data */
    model->mesh_count = primitive_count;
    model->meshes = calloc(primitive_count, sizeof(Mesh));
    model->mesh_albedo_idx = calloc(primitive_count, sizeof(size_t));
    for (size_t i = 0, mesh_idx = 0; i < data->meshes_count; i++) {
        for (size_t j = 0; j < data->meshes[i].primitives_count; j++) {
            /* only support triangles for now */
            if (data->meshes[i].primitives[j].type != cgltf_primitive_type_triangles) continue;

            Mesh *mesh = &model->meshes[mesh_idx];
            cgltf_primitive primitive = data->meshes[i].primitives[j];
            if (!load_attrs(primitive, mesh)) nob_return_defer(false);
            if (!load_indices(primitive, mesh)) nob_return_defer(false);

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
        free(model->meshes[i].indices);
        free(model->meshes[i].bone_ids);
        free(model->meshes[i].bone_weights);
    }
    free(model->bones.matrices);
    free(model->bones.rotations);
    free(model->bones.scales);
    free(model->bones.translations);
    free(model->bones.parent_idxs);
    free(model->meshes);
}

bool create_pipeline()
{
    /* create pipeline layout */
    VkPushConstantRange pk_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(Push_Const),
    };
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pk_range,
    };
    if (!vk_pl_layout_init(layout_ci, &gfx_pl_layout)) return false;

    /* create pipeline */ 
    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Gltf_Vertex, pos)
        },
        {
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Gltf_Vertex, normal)
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Gltf_Vertex),
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
        .position   = {0.0f, 15.0f, 15.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45,
        .projection = PERSPECTIVE,
    };

    Model robot = {.file_name = "res/robot.glb"};
    if (!load_gltf(&robot)) return 1;

    if (!init_window(800, 600, "gltf")) return 1;
    if (!create_pipeline()) return 1;

    for (size_t i = 0; i < robot.mesh_count; i++) {
        if (!vk_vtx_buff_staged_upload(&robot.meshes[i].vtx_buff, robot.meshes[i].vertices)) return 1;
        if (!vk_idx_buff_staged_upload(&robot.meshes[i].idx_buff, robot.meshes[i].indices))  return 1;
    }
    set_target_fps(120);

    while (!window_should_close()) {
        float dt = get_time();
        update_camera_free(&camera);

        begin_drawing(PURPLE);
        begin_mode_3d(camera);
            for (size_t i = 0; i < 6; i++) {
                push_matrix();
                    rotate_y(dt + (float)i);
                    translate(2.0, 0.0, 0.0);
                    draw_shape_wireframe(SHAPE_CUBE);
                pop_matrix();
            }

            scale(1.0, (sin(2.0f * dt) * 0.5 + 0.5) * 0.2 + 1.0, 1.0);

            /* draw model */
            Matrix mvp = {0};
            for (size_t i = 0; i < robot.mesh_count; i++) {
                push_matrix();
                    add_matrix(robot.bones.matrices[i]);
                    if (!get_mvp(&mvp)) return 1;
                    float16 f16_mvp = MatrixToFloatV(mvp);
                    Push_Const pk = {
                        .mvp = f16_mvp,
                        .color = robot.albedos[robot.mesh_albedo_idx[i]],
                    };
                    Vk_Buffer vtx_buff = robot.meshes[i].vtx_buff;
                    Vk_Buffer idx_buff = robot.meshes[i].idx_buff;
                    vk_draw_w_push_const(gfx_pl, gfx_pl_layout, vtx_buff, idx_buff, &pk, sizeof(pk));
                pop_matrix();
            }

        end_mode_3d();
        end_drawing();
    }

    wait_idle();
    for (size_t i = 0; i < robot.mesh_count; i++) {
        vk_buff_destroy(&robot.meshes[i].vtx_buff);
        vk_buff_destroy(&robot.meshes[i].idx_buff);
    }
    vk_destroy_pl_res(gfx_pl, gfx_pl_layout);
    unload_model(&robot);
    close_window();
    return 0;
}
