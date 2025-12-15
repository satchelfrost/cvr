#include "cvr.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "../nob.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h" // https://github.com/jkuhlmann/cgltf

#define PROGRESS_BAR 20

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
    VkDescriptorSet ds;
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
} glTF_Primitives;

typedef struct {
    glTF_Primitives primitives;
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

typedef struct {
    Cvr_Image *items;
    size_t count;
    size_t capacity;
} glTF_Images;

typedef struct {
    /* CPU-relevant data.
     * can be freed once uploaded to GPU */
    String_Builder file_buffer;
    cgltf_data *gltf_data;

    /* GPU-relevant data */
    glTF_Meshes meshes;
    glTF_Textures textures;
    glTF_Images images;
} glTF_Model;

typedef struct {
    Rvk_Descriptor_Set_Layout matrices;
    Rvk_Descriptor_Set_Layout textures;
} Descriptor_Set_Layouts;

typedef struct {
    VkPipeline pl;
    VkPipelineLayout pl_layout;
    Descriptor_Set_Layouts ds_layouts;
} Pipeline;

Pipeline scene = {0};

typedef struct {
    float16 proj;
    float16 view;
    Vector4 light_pos;
    Vector4 view_pos;
} UBO_Data;

typedef struct {
    UBO_Data data;
    Rvk_Buffer buff;
    VkDescriptorSet ds;
} UBO;

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

void populate_vertices(glTF_Primitive *primitive, cgltf_attribute attribute)
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

bool load_model_into_memory(const char *file_name, glTF_Model *model, bool print_progress)
{
    if (!read_entire_file(file_name, &model->file_buffer)) return 1;
    cgltf_options options = {0};
    cgltf_result res = cgltf_parse(&options, model->file_buffer.items, model->file_buffer.count, &model->gltf_data);
    if (res != cgltf_result_success) {
        printf("failed to parse %s: error %s\n", file_name, cgltf_res_to_str(res));
        return false;
    }
    res = cgltf_load_buffers(&options, model->gltf_data, file_name);
    if (res != cgltf_result_success) {
        printf("failed to load buffers: error %s\n", cgltf_res_to_str(res));
        return false;
    }

    if (print_progress) printf("loading meshes...\n");
    for (size_t m = 0; m < model->gltf_data->meshes_count; m++) {

        glTF_Mesh mesh = {0};

        for (size_t p = 0; p < model->gltf_data->meshes[m].primitives_count; p++) {
            cgltf_primitive primitive = model->gltf_data->meshes[m].primitives[p];
            assert(primitive.type == cgltf_primitive_type_triangles);

            /* interleave the attributes for this primitive */
            glTF_Primitive prim = {0};
            for (size_t a = 0; a < primitive.attributes_count; a++)
                populate_vertices(&prim, primitive.attributes[a]);

            /* grab material indices */
            cgltf_texture *texture = NULL;
            texture = primitive.material->pbr_metallic_roughness.base_color_texture.texture;
            if (texture) {
                prim.material.base_image_index = cgltf_image_index(model->gltf_data, texture->image);
                prim.material.flags |= MATERIAL_BASE;
            }
            texture = primitive.material->normal_texture.texture;
            if (texture) {
                prim.material.normal_image_index = cgltf_image_index(model->gltf_data, texture->image);
                prim.material.flags |= MATERIAL_METALLIC_ROUGHNESS;
            }
            texture = primitive.material->pbr_metallic_roughness.metallic_roughness_texture.texture;
            if (texture) {
                prim.material.metallic_roughness_image_index = cgltf_image_index(model->gltf_data, texture->image);
                prim.material.flags |= MATERIAL_NORMAL;
            }

            /* grab indices */
            assert(primitive.indices->component_type == cgltf_component_type_r_16u);
            uint16_t *indices = (uint16_t *)cgltf_buffer_view_data(primitive.indices->buffer_view);
            for (size_t i = 0; i < primitive.indices->count; i++)
                da_append(&prim.indices, indices[i]);

            da_append(&mesh.primitives, prim);
        }
        da_append(&model->meshes, mesh);
    }

    /* load images */
    if (print_progress) printf("loading images...\n");
    for (size_t i = 0; i < model->gltf_data->images_count; i++) {
        const char *image_path = temp_sprintf("res/%s", model->gltf_data->images[i].uri);

        if (print_progress) {
            float percentage = i / (float)model->gltf_data->images_count;
            size_t loaded = percentage*PROGRESS_BAR;
            printf("\r[");
            for (size_t j = 0; j < PROGRESS_BAR; j++) {
                printf("%s", (j <= loaded) ? "=" : ".");
            }
            printf("] %.f%%", (i == model->gltf_data->images_count - 1) ?  100 : percentage*100);
            fflush(stdout);
        }

        Cvr_Image image = load_image(image_path);
        da_append(&model->images, image);
    }
    if (print_progress) printf("\n");

    return true;
}

void setup_ds_layouts()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        { // set 0, binding 0 - model.vert.glsl
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        { // set 1, binding 0 - model.frag.glsl
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        { // set 1, binding 1 - model.frag.glsl
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    rvk_ds_layout_init(bindings + 0, 1, &scene.ds_layouts.matrices);
    rvk_ds_layout_init(bindings + 1, 2, &scene.ds_layouts.textures);
}

void update_ds(glTF_Model model, UBO ubo, Rvk_Descriptor_Pool_Arena arena)
{
    /* uniform buffer */
    rvk_descriptor_pool_arena_alloc_set(&arena, &scene.ds_layouts.matrices, &ubo.ds);
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &ubo.buff.info,
        .dstSet = ubo.ds,
    };
    rvk_update_ds(1, &write);

    /* material descriptor sets */
    for (size_t i = 0; i < model.meshes.count; i++) {
        glTF_Mesh mesh = model.meshes.items[i];
        for (size_t j = 0; j < mesh.primitives.count; j++) {
            rvk_descriptor_pool_arena_alloc_set(&arena, &scene.ds_layouts.textures, &mesh.primitives.items[j].material.ds);

            glTF_Primitive primitive = mesh.primitives.items[j];
            Rvk_Texture base_color_texture = model.textures.items[primitive.material.base_image_index];
            Rvk_Texture normal_texture = model.textures.items[primitive.material.normal_image_index];
            assert(base_color_texture.img.handle);
            assert(normal_texture.img.handle);

            VkWriteDescriptorSet writes[] = {
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &base_color_texture.info,
                    .dstSet = primitive.material.ds,
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &normal_texture.info,
                    .dstSet = primitive.material.ds,
                },
            };
            rvk_update_ds(ARRAY_LEN(writes), writes);
        }
    }
}

int main()
{
    glTF_Model model = {0};
    if (!load_model_into_memory("res/Sponza.gltf", &model, true)) return 1;

    /* initialize window/vulkan */
    init_window(1600, 900, "sponza");
    
    /* upload mesh to GPU */
    for (size_t i = 0; i < model.meshes.count; i++) {
        glTF_Mesh mesh = model.meshes.items[i];
        for (size_t j = 0; j < mesh.primitives.count; j++) {
            glTF_Primitive primitive = mesh.primitives.items[j];

            /* vertex buffer */
            size_t count = primitive.vertices.count;
            size_t size  = count * sizeof(*primitive.vertices.items);
            primitive.vtx_buff = rvk_create_vertex_buffer(size, count, primitive.vertices.items);

            /* index buffer */
            count = primitive.indices.count;
            size  = count * sizeof(*primitive.indices.items);
            primitive.idx_buff = rvk_create_index_buffer(size, count, primitive.indices.items);
        }
    }

    /* upload textures to GPU */
    for (size_t i = 0; i < model.images.count; i++) {
        Rvk_Texture texture = load_texture(model.images.items[i]);
        da_append(&model.textures, texture);
        // TODO: unload image
    }

    // TODO: we could free the gltf data here now that stuff is on the GPU

    UBO ubo = {0};
    rvk_uniform_buff_init(sizeof(ubo.data), &ubo.data, &ubo.buff);
    rvk_buff_map(&ubo.buff);
    memcpy(ubo.buff.mapped, &ubo.data, sizeof(ubo.data));

    /* upload vulkan resources */
    Rvk_Descriptor_Pool_Arena ds_pool_arena = rvk_create_descriptor_pool_arena();
    setup_ds_layouts();
    update_ds(model, ubo, ds_pool_arena);
    // create_pipeline();

    // close_window();

    return 0;
}
