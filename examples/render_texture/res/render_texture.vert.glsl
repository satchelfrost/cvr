#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 out_color;

void main()
{
    gl_Position = push_const.mvp * vec4(position, 1.0);
    out_color = color;
}
