#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 tex_coord;

layout(binding = 0) uniform sampler2D y_sampler;
layout(binding = 1) uniform sampler2D cb_sampler;
layout(binding = 2) uniform sampler2D cr_sampler;

layout(location = 0) out vec4 out_color;

mat4 rec601 = mat4(
    1.16438,  0.00000,  1.59603, -0.87079,
    1.16438, -0.39176, -0.81297,  0.52959,
    1.16438,  2.01723,  0.00000, -1.08139,
    0, 0, 0, 1
);

vec3 srgb_to_linear(vec3 srgb)
{
    return mix(srgb / 12.92, pow((srgb + 0.055) / 1.055, vec3(2.4)), step(0.04045, srgb));
}

void main()
{
    float y  = texture(y_sampler,  tex_coord).r;
    float cb = texture(cb_sampler, tex_coord).r;
    float cr = texture(cr_sampler, tex_coord).r;
    out_color = vec4(y, cb, cr, 1.0) * rec601;
    out_color.rgb = srgb_to_linear(out_color.rgb);
}
