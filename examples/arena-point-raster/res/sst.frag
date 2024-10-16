#version 450

layout(binding = 0) uniform sampler2D sampler_color;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_color;

vec3 srgb_to_linear(vec3 srgb)
{
    return mix(srgb / 12.92, pow((srgb + 0.055) / 1.055, vec3(2.4)), step(0.04045, srgb));
}

void main()
{
    //out_color = texture(sampler_color, vec2(uv.s, 1.0 - uv.t)); // THANKS BERK!!!
    out_color = texture(sampler_color, uv);
    out_color.rgb = srgb_to_linear(out_color.rgb);
}
