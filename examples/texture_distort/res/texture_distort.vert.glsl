#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec3 out_color;

layout(binding = 0) uniform floats {
    int keypress;
    int index;
    float speed;
} ubo;

layout(std430, binding = 1) buffer depth_data {
   vec2 offsets[ ];
};

#define KEYPRESS_LEFT  1
#define KEYPRESS_RIGHT 2
#define KEYPRESS_DOWN  4
#define KEYPRESS_UP    8

void main()
{
    if ((ubo.keypress & KEYPRESS_LEFT)  > 0) offsets[ubo.index].x -= ubo.speed;
    if ((ubo.keypress & KEYPRESS_RIGHT) > 0) offsets[ubo.index].x += ubo.speed;
    if ((ubo.keypress & KEYPRESS_DOWN)  > 0) offsets[ubo.index].y += ubo.speed;
    if ((ubo.keypress & KEYPRESS_UP)    > 0) offsets[ubo.index].y -= ubo.speed;
    vec2 pos = position + offsets[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    out_uv = uv;
    out_color = color;
}
