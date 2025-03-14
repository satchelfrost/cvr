#version 450

// #extension GL_ARB_gpu_shader_int64 : enable
// #extension GL_EXT_shader_atomic_int64 : enable

struct Vertex {
    float x, y, z;
    uint color;
};

#define NUM_CCTVS 4

layout(binding = 0) uniform uniform_data {
    mat4 mvps[NUM_CCTVS]; // cctv camera's view-proj w/ model
    vec2 img_sizes[NUM_CCTVS]; 
} ubo;

layout(std430, binding = 1) buffer vert_data {
   Vertex vertices[ ];
};

layout(std430, binding = 2) buffer depth_data {
   uint depth_buffs[ ];
};

layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform constants
{
    uint offset;
    uint point_count;
} push_const;

void main()
{
    uint index = gl_GlobalInvocationID.x + push_const.offset;
    if (index > push_const.point_count) return;
    Vertex vert = vertices[index];

    int offset = 0;
    for (int i = 0; i < 1; i++) {
        vec4 cam_clip = ubo.mvps[i] * vec4(vert.x, vert.y, vert.z, 1.0);
        bool cam_sees = (-cam_clip.w < cam_clip.x && cam_clip.x < cam_clip.w) &&
                        (-cam_clip.w < cam_clip.y && cam_clip.y < cam_clip.w) &&
                        (-cam_clip.w < cam_clip.z && cam_clip.z < cam_clip.w);
        if (!cam_sees) continue;

        vec3 ndc = cam_clip.xyz / cam_clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;
        // ivec2 pixel_coords = ivec2(uv * ubo.img_sizes[i]);
        // ivec2 img_size = ivec2(ubo.img_sizes[i]);
        vec2 img_sizes = vec2(1280.0, 960.0);
        ivec2 pixel_coords = ivec2(uv * img_sizes);
        ivec2 img_size = ivec2(img_sizes);
        int pixel_id = pixel_coords.x + pixel_coords.y * img_size.x;
        // pixel_id += offset;
        uint depth = floatBitsToUint(cam_clip.w);

        if (depth < depth_buffs[pixel_id])
            atomicMin(depth_buffs[pixel_id], depth);

        // offset += img_size.x * img_size.y;
    }
}
