#version 450

#extension GL_ARB_gpu_shader_int64 : enable

struct Vertex {
    float x, y, z;
    uint colors;
};

layout(binding = 0) uniform uniform_data {
    mat4 mvp;
} ubo;

layout(std430, binding = 1) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(rgba8, binding = 2) uniform image2D out_img;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;

    ivec2 img_size  = imageSize(out_img);
    if (id.x >= img_size.x) return;

    ivec2 pixel_coords = ivec2(id);
    int pixel_id = pixel_coords.x + pixel_coords.y * img_size.x;

    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
    uint tmp_color = uint(frame_buff[pixel_id] & 0x00FFFFFFUL);
    if (tmp_color != 0)
        color = vec4(unpackUnorm4x8(tmp_color));

    imageStore(out_img, pixel_coords, color);
    //frame_buff[pixel_id] = 0xffffffffff000010UL;
}
