#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

layout(binding = 0) uniform CameraMVPs
{
    mat4 mvp1;
    mat4 mvp2;
    mat4 mvp3;
    int cam_idx;
} cameras;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uvec3 inColor;

layout(location = 0) out vec3 fragColor;

void main()
{
    vec4 cam1_rel_pos = cameras.mvp1 * vec4(inPosition, 1.0);
    vec4 cam2_rel_pos = cameras.mvp2 * vec4(inPosition, 1.0);
    vec4 cam3_rel_pos = cameras.mvp3 * vec4(inPosition, 1.0);

    int count = 0;
    float r = 0.0;
    float g = 0.0;
    float b = 0.0;
    if ((-cam1_rel_pos.w < cam1_rel_pos.x && cam1_rel_pos.x < cam1_rel_pos.w) &&
        (-cam1_rel_pos.w < cam1_rel_pos.y && cam1_rel_pos.y < cam1_rel_pos.w) &&
        (-cam1_rel_pos.w < cam1_rel_pos.z && cam1_rel_pos.z < cam1_rel_pos.w)) {
        r = 1.0;
        count++;
    }
    if ((-cam2_rel_pos.w < cam2_rel_pos.x && cam2_rel_pos.x < cam2_rel_pos.w) &&
        (-cam2_rel_pos.w < cam2_rel_pos.y && cam2_rel_pos.y < cam2_rel_pos.w) &&
        (-cam2_rel_pos.w < cam2_rel_pos.z && cam2_rel_pos.z < cam2_rel_pos.w)) {
        g = 1.0;
        count++;
    }
    if ((-cam3_rel_pos.w < cam3_rel_pos.x && cam3_rel_pos.x < cam3_rel_pos.w) &&
        (-cam3_rel_pos.w < cam3_rel_pos.y && cam3_rel_pos.y < cam3_rel_pos.w) &&
        (-cam3_rel_pos.w < cam3_rel_pos.z && cam3_rel_pos.z < cam3_rel_pos.w)) {
        b = 1.0;
        count++;
    }

    if (count == 0)
        fragColor = vec3(inColor) / 255.0;
    else
        fragColor = vec3(r, g, b);

    gl_PointSize = 3.0;
    gl_Position = push_const.mvp * vec4(inPosition, 1.0);
}
