#version 450

layout(push_constant) uniform constants {
    mat4 mvp;
    vec4 color;
} push_const;

layout(location = 0) in vec3 position;
layout(location = 0) out vec4 out_color;

void main()
{
    gl_Position = push_const.mvp * vec4(position, 1.0);
    out_color = push_const.color;
}
