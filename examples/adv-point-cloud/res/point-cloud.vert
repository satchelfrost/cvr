#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

/*
* Shader Modes
* -----
* 0 - Base model mode
* 1 - Progressive color based on number of cameras that can see a point
* 2 - Single texture mode, i.e. texture from single cctv
* 3 - Multi-texture mode
*/
layout(binding = 0) uniform CameraMVPs
{
    mat4 mvps[4];
    int idx; // closest camera
    int shader_mode;
} cameras;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in uvec3 in_color;

layout(location = 0) out vec3 model_color;
layout(location = 1) out vec3 debug_color;
layout(location = 2) out int closest_cam_idx;
layout(location = 3) out int shader_mode;
layout(location = 4) out int use_cam_texture;
layout(location = 5) out int cam_sees[4];
layout(location = 9) out vec3 ndcs[4];

void main()
{
    gl_PointSize = 3.0;
    gl_Position = push_const.mvp * vec4(in_pos, 1.0);
    shader_mode = cameras.shader_mode;
    closest_cam_idx = cameras.idx;

    /* progressively color white the more cameras can see point */
    vec4 cam0_pos = cameras.mvps[0] * vec4(in_pos, 1.0);
    vec4 cam1_pos = cameras.mvps[1] * vec4(in_pos, 1.0);
    vec4 cam2_pos = cameras.mvps[2] * vec4(in_pos, 1.0);
    vec4 cam3_pos = cameras.mvps[3] * vec4(in_pos, 1.0);
    ndcs[0] = cam0_pos.xyz / cam0_pos.w;
    ndcs[1] = cam1_pos.xyz / cam1_pos.w;
    ndcs[2] = cam2_pos.xyz / cam2_pos.w;
    ndcs[3] = cam3_pos.xyz / cam3_pos.w;
    use_cam_texture = 0;
    cam_sees[0] = int((-cam0_pos.w < cam0_pos.x && cam0_pos.x < cam0_pos.w) &&
                      (-cam0_pos.w < cam0_pos.y && cam0_pos.y < cam0_pos.w) &&
                      (-cam0_pos.w < cam0_pos.z && cam0_pos.z < cam0_pos.w));
    cam_sees[1] = int((-cam1_pos.w < cam1_pos.x && cam1_pos.x < cam1_pos.w) &&
                      (-cam1_pos.w < cam1_pos.y && cam1_pos.y < cam1_pos.w) &&
                      (-cam1_pos.w < cam1_pos.z && cam1_pos.z < cam1_pos.w));
    use_cam_texture = (cam_sees[1] == 1) ? 1 : use_cam_texture;
    cam_sees[2] = int((-cam2_pos.w < cam2_pos.x && cam2_pos.x < cam2_pos.w) &&
                      (-cam2_pos.w < cam2_pos.y && cam2_pos.y < cam2_pos.w) &&
                      (-cam2_pos.w < cam2_pos.z && cam2_pos.z < cam2_pos.w));
    use_cam_texture = (cam_sees[2] == 1) ? 2 : use_cam_texture;
    cam_sees[3] = int((-cam3_pos.w < cam3_pos.x && cam3_pos.x < cam3_pos.w) &&
                      (-cam3_pos.w < cam3_pos.y && cam3_pos.y < cam3_pos.w) &&
                      (-cam3_pos.w < cam3_pos.z && cam3_pos.z < cam3_pos.w));
    use_cam_texture = (cam_sees[3] == 1) ? 3 : use_cam_texture;
    float brightness = cam_sees[0] * 0.25 + cam_sees[1] * 0.25 + cam_sees[2] * 0.25 + cam_sees[3] * 0.25;
    model_color = vec3(in_color) / 255.0;
    debug_color = vec3(brightness, brightness, brightness);
}
