#version 450

layout (location = 0) in  vec3 in_pos;
layout (location = 1) in  vec3 in_color;
layout (location = 0) out vec3 out_color;

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

void main()
{
    gl_Position = push_const.mvp * vec4(in_pos, 1.0);
    out_color = in_color;
}
