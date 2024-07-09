#version 450

layout(location = 0) in vec3 model_color;
layout(location = 1) in vec3 debug_color;
layout(location = 2) flat in int closest_cam_idx;
layout(location = 3) flat in int shader_mode;
layout(location = 4) flat in int use_cam_texture;
layout(location = 5) flat in int cam_sees[4];
layout(location = 9) in vec3 ndcs[4];


layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D tex_sampler;

void main()
{
    float dist = length(2 * gl_PointCoord - 1.0);
    if (dist > 1.0)
        discard;

    /* Make the texture get the color of camera that can see it */
    //vec2 uv_0 = (ndcs[0].xy * 0.5) + 0.5;
    //if (0.0 < uv_0.x && uv_0.x < 1.0 && 0.0 < uv_0.y && uv_0.y < 1.0 && cam_sees[0] > 0)
    //    out_color = vec4(1.0, 0.0, 0.0, 1.0);
    //vec2 uv_1 = (ndcs[1].xy * 0.5) + 0.5;
    //if (0.0 < uv_1.x && uv_1.x < 1.0 && 0.0 < uv_1.y && uv_1.y < 1.0 && cam_sees[1] > 0)
    //    out_color = vec4(1.0, 1.0, 0.0, 1.0);
    //vec2 uv_2 = (ndcs[2].xy * 0.5) + 0.5;
    //if (0.0 < uv_2.x && uv_2.x < 1.0 && 0.0 < uv_2.y && uv_2.y < 1.0 && cam_sees[2] > 0)
    //    out_color = vec4(0.0, 1.0, 0.0, 1.0);
    //vec2 uv_3 = (ndcs[3].xy * 0.5) + 0.5;
    //if (0.0 < uv_3.x && uv_3.x < 1.0 && 0.0 < uv_3.y && uv_3.y < 1.0 && cam_sees[3] > 0)
    //    out_color = vec4(1.0, 0.0, 1.0, 1.0);

    if (cam_sees[0] > 0)
        out_color = vec4(1.0, 0.0, 0.0, 1.0);
    if (cam_sees[1] > 0)
        out_color = vec4(1.0, 1.0, 0.0, 1.0);
    if (cam_sees[2] > 0)
        out_color = vec4(0.0, 1.0, 0.0, 1.0);
    if (cam_sees[3] > 0)
        out_color = vec4(1.0, 0.0, 1.0, 1.0);

    int cam_count = cam_sees[0] + cam_sees[1] + cam_sees[2] + cam_sees[3];
    if (cam_count > 0) {
        vec2 uv = (ndcs[closest_cam_idx].xy * 0.5) + 0.5;
        vec4 tex_color = textureLod(tex_sampler, uv, 0.0);
        if (0.0 < uv.x && uv.x < 1.0 && 0.0 < uv.y && uv.y < 1.0 && cam_sees[closest_cam_idx] > 0)
            out_color = tex_color;

    } else {
        out_color = vec4(model_color, 1.0);
    }


    if (shader_mode == 1)
        out_color = vec4(debug_color, 1.0);
    else if (shader_mode == 0)
        out_color = vec4(model_color, 1.0);

    /* branchless rewrite */
    //out_color = (shader_mode == 0) ? vec4(model_color, 1.0) : (shader_mode == 1) ? vec4(debug_color) : out_color;
}
