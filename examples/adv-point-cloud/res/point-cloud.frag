#version 450

layout(location = 0) in vec3 model_color;
layout(location = 1) in vec3 debug_color;
layout(location = 2) flat in int shader_mode;
layout(location = 3) flat in int closest_cam_idx;
layout(location = 4) flat in int cam_sees[4];
layout(location = 8) in vec3 ndcs[4];
layout(location = 12) flat in int cam_order[4];

layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D tex_sampler_0;
layout(set = 1, binding = 1) uniform sampler2D tex_sampler_1;
layout(set = 1, binding = 2) uniform sampler2D tex_sampler_2;
layout(set = 1, binding = 3) uniform sampler2D tex_sampler_3;

vec4 tex_colors[4];

void main()
{
    float dist = length(2 * gl_PointCoord - 1.0);
    if (dist > 1.0)
        discard;

    vec2 uv_0 = (ndcs[0].xy * 0.5) + 0.5;
    vec2 uv_1 = (ndcs[1].xy * 0.5) + 0.5;
    vec2 uv_2 = (ndcs[2].xy * 0.5) + 0.5;
    vec2 uv_3 = (ndcs[3].xy * 0.5) + 0.5;
    tex_colors[0] = textureLod(tex_sampler_0, uv_0, 0.0);
    tex_colors[1] = textureLod(tex_sampler_1, uv_1, 0.0);
    tex_colors[2] = textureLod(tex_sampler_2, uv_2, 0.0);
    tex_colors[3] = textureLod(tex_sampler_3, uv_3, 0.0);

    switch (shader_mode) {
    case 0:
        out_color = vec4(model_color, 1.0);
        break;
    case 1:
        out_color = vec4(debug_color, 1.0);
        break;
    case 2:
        out_color = (cam_sees[cam_order[0]] > 0) ? tex_colors[cam_order[0]] : vec4(model_color, 1.0);
        break;
    case 3:
        if (cam_sees[cam_order[3]] > 0) out_color = tex_colors[cam_order[3]];
        if (cam_sees[cam_order[2]] > 0) out_color = tex_colors[cam_order[2]];
        if (cam_sees[cam_order[1]] > 0) out_color = tex_colors[cam_order[1]];
        if (cam_sees[cam_order[0]] > 0) out_color = tex_colors[cam_order[0]];
        int cam_count = cam_sees[0] + cam_sees[1] + cam_sees[2] + cam_sees[3];
        if (cam_count == 0) out_color = vec4(model_color, 1.0);
    }
}
