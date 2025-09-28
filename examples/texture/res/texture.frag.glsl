#version 450

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_color;

float radius = 50.0f;

vec2 tl = vec2(0);
vec2 tr = vec2(1170,0);
vec2 bl = vec2(0, 400);
vec2 br = vec2(1170, 400);

void main()
{
    vec2 coords = uv * vec2(1170, 400);
    bool in_circles = length(coords - vec2(radius, radius))              < radius ||
                      length(coords - vec2(1170 - radius, radius))       < radius ||
                      length(coords - vec2(radius, 400 - radius))        < radius ||
                      length(coords - vec2(1170 - radius, 400 - radius)) < radius;
    bool in_cutout = (coords.x > radius && coords.x < 1170 - radius) ||
                          (coords.y > radius && coords.y < 400 - radius);

    // if (in_circles) out_color =  texture(tex_sampler, uv);
    // if (in_cutout) out_color =  texture(tex_sampler, uv);
    // if (in_circles || in_cutout) out_color =  texture(tex_sampler, uv);
    // if (in_circles && in_cutout) out_color =  texture(tex_sampler, uv);
    // if ((in_circles || in_cutout) && !(in_circles && in_cutout)) out_color =  texture(tex_sampler, uv);
    out_color =  texture(tex_sampler, uv);
    // else discard;

}
