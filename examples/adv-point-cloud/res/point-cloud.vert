#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

/*
* Shader Modes
* -----
* 0 - Progressive color based on number of cameras that can see a point
* 1 - Single texture mode, i.e. texture from single cctv
* 2 - Multi-texture mode
*/
layout(binding = 0) uniform CameraMVPs
{
    mat4 mvps[4];
    int idx;
    int shader_mode;
} cameras;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in uvec3 in_color;

layout(location = 0) out vec3 model_color;
layout(location = 1) out vec3 ndc;
layout(location = 2) out vec3 debug_color;
layout(location = 3) out int show_tex;
layout(location = 4) out int shader_mode;

void main()
{
    gl_PointSize = 3.0;
    gl_Position = push_const.mvp * vec4(in_pos, 1.0);
    shader_mode = cameras.shader_mode;

    /* test the current cctv */
    vec4 cam_rel_pos = cameras.mvps[cameras.idx] * vec4(in_pos, 1.0);
    ndc = cam_rel_pos.xyz / cam_rel_pos.w;
    show_tex = int((-cam_rel_pos.w < cam_rel_pos.x && cam_rel_pos.x < cam_rel_pos.w) &&
                   (-cam_rel_pos.w < cam_rel_pos.y && cam_rel_pos.y < cam_rel_pos.w) &&
                   (-cam_rel_pos.w < cam_rel_pos.z && cam_rel_pos.z < cam_rel_pos.w));

    /* progressively color white the more cameras can see point */
    vec4 cam1_rel_pos = cameras.mvps[0] * vec4(in_pos, 1.0);
    vec4 cam2_rel_pos = cameras.mvps[1] * vec4(in_pos, 1.0);
    vec4 cam3_rel_pos = cameras.mvps[2] * vec4(in_pos, 1.0);
    vec4 cam4_rel_pos = cameras.mvps[3] * vec4(in_pos, 1.0);
    int cam_1_sees = int((-cam1_rel_pos.w < cam1_rel_pos.x && cam1_rel_pos.x < cam1_rel_pos.w) &&
                         (-cam1_rel_pos.w < cam1_rel_pos.y && cam1_rel_pos.y < cam1_rel_pos.w) &&
                         (-cam1_rel_pos.w < cam1_rel_pos.z && cam1_rel_pos.z < cam1_rel_pos.w));
    int cam_2_sees = int((-cam2_rel_pos.w < cam2_rel_pos.x && cam2_rel_pos.x < cam2_rel_pos.w) &&
                         (-cam2_rel_pos.w < cam2_rel_pos.y && cam2_rel_pos.y < cam2_rel_pos.w) &&
                         (-cam2_rel_pos.w < cam2_rel_pos.z && cam2_rel_pos.z < cam2_rel_pos.w));
    int cam_3_sees = int((-cam3_rel_pos.w < cam3_rel_pos.x && cam3_rel_pos.x < cam3_rel_pos.w) &&
                         (-cam3_rel_pos.w < cam3_rel_pos.y && cam3_rel_pos.y < cam3_rel_pos.w) &&
                         (-cam3_rel_pos.w < cam3_rel_pos.z && cam3_rel_pos.z < cam3_rel_pos.w));
    int cam_4_sees = int((-cam4_rel_pos.w < cam4_rel_pos.x && cam4_rel_pos.x < cam4_rel_pos.w) &&
                         (-cam4_rel_pos.w < cam4_rel_pos.y && cam4_rel_pos.y < cam4_rel_pos.w) &&
                         (-cam4_rel_pos.w < cam4_rel_pos.z && cam4_rel_pos.z < cam4_rel_pos.w));
    float brightness = cam_1_sees * 0.25 + cam_2_sees * 0.25 + cam_3_sees * 0.25 + cam_4_sees * 0.25;
    model_color = vec3(in_color) / 255.0;
    debug_color = vec3(brightness, brightness, brightness);
}
