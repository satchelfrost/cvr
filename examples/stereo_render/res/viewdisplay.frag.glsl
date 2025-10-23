#version 450

layout (binding = 0) uniform sampler2DArray sampler_view;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

layout(push_constant) uniform constants
{
    uint view_layer;
} push_const;

void main()
{
    out_color = texture(sampler_view, vec3(in_uv, push_const.view_layer));
}
