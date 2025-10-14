#version 450

#define NEAR 0.01
#define FAR 1000.0

#extension GL_ARB_gpu_shader_int64 : enable

layout(binding = 0) uniform uniform_data {
    mat4 mvp;
    int width;
    int height;
} ubo;

layout(std430, binding = 1) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(binding = 2) uniform sampler2D depth_buffer;
layout(binding = 3) uniform sampler2D color_buffer;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


float linearize_depth(float depth)
{
    float n = NEAR;
    float f = FAR;
    float z = depth;
    return (2.0 * n) / (f + n - z * (f - n));
}

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;
    ivec2 img_size  = ivec2(ubo.width, ubo.height);
    if (id.x >= img_size.x) return;

    ivec2 pixel_coords = ivec2(id);
    int pixel_id = pixel_coords.x + pixel_coords.y * img_size.x;


    float depth = texelFetch(depth_buffer, ivec2(gl_GlobalInvocationID.xy), 0).r;
    vec4 color = texelFetch(color_buffer, ivec2(gl_GlobalInvocationID.xy), 0);

    uint uint_color = uint(color.a*255.0) << 24 | uint(color.b*255.0) << 16 | uint(color.g*255) << 8 | uint(color.r*255.0);
    uint64_t depth64 = floatBitsToUint((linearize_depth(depth)) * (FAR - NEAR));
    // uint clr_color = 0xff444418;
    // uint color = 0xff000000;
    if (depth == 1.0) uint_color = 0xff000000;
    frame_buff[pixel_id] = depth64 << 32 | uint64_t(uint_color);
    // frame_buff[pixel_id] = 0xffffffffff000000UL;
}
