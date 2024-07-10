#version 450

layout(location = 0) in vec3 model_color;
layout(location = 1) in vec3 debug_color;
layout(location = 2) flat in int closest_cam_idx;
layout(location = 3) flat in int shader_mode;
layout(location = 4) flat in int use_cam_texture;
layout(location = 5) flat in int cam_sees[4];
layout(location = 9) in vec3 ndcs[4];


layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D tex_sampler_0;
layout(set = 1, binding = 1) uniform sampler2D tex_sampler_1;
layout(set = 1, binding = 2) uniform sampler2D tex_sampler_2;
layout(set = 1, binding = 3) uniform sampler2D tex_sampler_3;

void main()
{
    float dist = length(2 * gl_PointCoord - 1.0);
    if (dist > 1.0)
        discard;

    vec2 uv_0 = (ndcs[0].xy * 0.5) + 0.5;
    vec2 uv_1 = (ndcs[1].xy * 0.5) + 0.5;
    vec2 uv_2 = (ndcs[2].xy * 0.5) + 0.5;
    vec2 uv_3 = (ndcs[3].xy * 0.5) + 0.5;
    vec4 tex_color_0 = textureLod(tex_sampler_0, uv_0, 0.0);
    vec4 tex_color_1 = textureLod(tex_sampler_1, uv_1, 0.0);
    vec4 tex_color_2 = textureLod(tex_sampler_2, uv_2, 0.0);
    vec4 tex_color_3 = textureLod(tex_sampler_3, uv_3, 0.0);
    if (cam_sees[0] > 0) out_color = tex_color_0;
    if (cam_sees[1] > 0) out_color = tex_color_1;
    if (cam_sees[2] > 0) out_color = tex_color_1;
    if (cam_sees[3] > 0) out_color = tex_color_2;

    int cam_count = cam_sees[0] + cam_sees[1] + cam_sees[2] + cam_sees[3];
    if (cam_count > 1) {
        if (cam_sees[closest_cam_idx] > 0) {
            if (closest_cam_idx == 0)
                out_color = tex_color_0;
            else if (closest_cam_idx == 1)
                out_color = tex_color_1;
            else if (closest_cam_idx == 2)
                out_color = tex_color_2;
            else
                out_color = tex_color_3;
        } else {
            if (cam_sees[0] > 0)
                out_color = tex_color_0;
            else if (cam_sees[1] > 0)
                out_color = tex_color_1;
            else if (cam_sees[2] > 0)
                out_color = tex_color_2;
            else if (cam_sees[3] > 0)
                out_color = tex_color_3;
        }
    } else {
        if (cam_count == 1) {
            if (cam_sees[0] > 0)
                out_color = tex_color_0;
            else if (cam_sees[1] > 0)
                out_color = tex_color_1;
            else if (cam_sees[2] > 0)
                out_color = tex_color_2;
            else if (cam_sees[3] > 0)
                out_color = tex_color_3;
        } else {
            out_color = vec4(model_color, 1.0);
        }
    }


    if (shader_mode == 1)
        out_color = vec4(debug_color, 1.0);
    else if (shader_mode == 0)
        out_color = vec4(model_color, 1.0);

    /* branchless rewrite */
    //out_color = (shader_mode == 0) ? vec4(model_color, 1.0) : (shader_mode == 1) ? vec4(debug_color) : out_color;
}
