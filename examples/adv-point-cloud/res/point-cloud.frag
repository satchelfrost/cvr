#version 450

layout(location = 0) in vec3 model_color;
layout(location = 1) in vec3 ndc;
layout(location = 2) in vec3 debug_color;
layout(location = 3) flat in int show_tex;
layout(location = 4) flat in int shader_mode;

layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D tex_sampler;

void main()
{
    float dist = length(2 * gl_PointCoord - 1.0);
    if (dist > 1.0)
        discard;

    vec2 uv  = (ndc.xy * 0.5) + 0.5;

    vec4 tex_color = textureLod(tex_sampler, uv, 0.0);
    if (0.0 < uv.x && uv.x < 1.0 && 0.0 < uv.y && uv.y < 1.0 && show_tex > 0)
        out_color = tex_color;
    else
        out_color = vec4(model_color, 1.0);

    if (shader_mode == 0)
        out_color = vec4(debug_color, 1.0);
}
