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

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;
    ivec2 img_size  = ivec2(ubo.width, ubo.height);
    if (id.x >= img_size.x) return;

    ivec2 pixel_coords = ivec2(id);
    int pixel_id = pixel_coords.x + pixel_coords.y * img_size.x;
    frame_buff[pixel_id] = 0xffffffffff444418UL;
}
