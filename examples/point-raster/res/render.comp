#version 450

#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable

layout(push_constant) uniform constants
{
    uint offset;
} push_const;

struct Vertex {
    float x, y, z;
    uint colors;
};

layout(binding = 0) uniform uniform_data {
    mat4 mvp;
} ubo;

layout(std430, binding = 1) buffer vert_data {
   Vertex vertices[ ];
};

layout(std430, binding = 2) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

void main()
{
    Vertex vert = vertices[gl_GlobalInvocationID.x + push_const.offset];
    vec4 pos = ubo.mvp * vec4(vert.x, vert.y, vert.z, 1.0);
    vec3 ndc = pos.xyz / pos.w;

    if (pos.w <= 0 || ndc.x < -1.0 || ndc.x > 1.0 || ndc.y < -1.0 || ndc.y > 1.0)
        return;

    ivec2 img_size = ivec2(2048, 2048);
    vec2 img_pos = (ndc.xy * 0.5 + 0.5) * img_size;

    ivec2 pixel_coords = ivec2(img_pos);
    int pixel_id = pixel_coords.x + pixel_coords.y * img_size.x;

    uint64_t depth = floatBitsToUint(pos.w);
    uint64_t old_depth = frame_buff[pixel_id] >> 32;

    if (depth < old_depth) {
        uint64_t packed = depth << 32 | uint64_t(vert.colors);
        atomicMin(frame_buff[pixel_id], packed);
    }
}
