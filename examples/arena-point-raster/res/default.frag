#version 450

layout(binding = 0) uniform sampler2D sampler_color;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_color;

void main()
{
    //out_color = texture(sampler_color, vec2(uv.s, 1.0 - uv.t)); // THANKS BERK!!!
    out_color = texture(sampler_color, uv);
}
