#version 450

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

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


float linearize_depth(float depth)
{
  float n = 0.01;
  float f = 1000.0;
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
    uint64_t depth64 = floatBitsToUint((linearize_depth(depth)) * 1000);
    // uint clr_color = 0xff444418;
    uint clr_color = 0xff000000;
    frame_buff[pixel_id] = depth64 << 32 | uint64_t(clr_color);
    // frame_buff[pixel_id] = 0xffffffffff000000UL;
}
