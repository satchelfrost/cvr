#version 450

#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_uv;

layout (location = 0) out vec3 out_color;

layout (binding = 0) uniform UBO {
    mat4 mvps[2];
} ubo;

void main()
{
    out_color = in_color;
    gl_Position = ubo.mvps[gl_ViewIndex] * vec4(in_pos, 1.0);
}
