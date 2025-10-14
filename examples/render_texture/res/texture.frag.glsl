#version 450

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(tex_sampler, uv);
    // out_color = vec4(1.0, 0.0, 0.0, 1.0);
}
