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
layout(location = 1) out vec3 ndc;
layout(location = 2) out vec3 debugColor;

void main()
{
    vec4 cam1_rel_pos = cameras.mvp1 * vec4(inPosition, 1.0);
    ndc = cam1_rel_pos.xyz / cam1_rel_pos.w;
    vec4 cam2_rel_pos = cameras.mvp2 * vec4(inPosition, 1.0);
    vec4 cam3_rel_pos = cameras.mvp3 * vec4(inPosition, 1.0);

    debugColor = vec3(0.0, 0.0, 0.0);
    if ((-cam1_rel_pos.w < cam1_rel_pos.x && cam1_rel_pos.x < cam1_rel_pos.w) &&
        (-cam1_rel_pos.w < cam1_rel_pos.y && cam1_rel_pos.y < cam1_rel_pos.w) &&
        (-cam1_rel_pos.w < cam1_rel_pos.z && cam1_rel_pos.z < cam1_rel_pos.w)) {
        debugColor.x = 1.0;
    }
    if ((-cam2_rel_pos.w < cam2_rel_pos.x && cam2_rel_pos.x < cam2_rel_pos.w) &&
        (-cam2_rel_pos.w < cam2_rel_pos.y && cam2_rel_pos.y < cam2_rel_pos.w) &&
        (-cam2_rel_pos.w < cam2_rel_pos.z && cam2_rel_pos.z < cam2_rel_pos.w)) {
        debugColor.y = 1.0;
    }
    if ((-cam3_rel_pos.w < cam3_rel_pos.x && cam3_rel_pos.x < cam3_rel_pos.w) &&
        (-cam3_rel_pos.w < cam3_rel_pos.y && cam3_rel_pos.y < cam3_rel_pos.w) &&
        (-cam3_rel_pos.w < cam3_rel_pos.z && cam3_rel_pos.z < cam3_rel_pos.w)) {
        debugColor.z = 1.0;
    }

    if (debugColor == vec3(0.0, 0.0, 0.0))
        fragColor = vec3(inColor) / 255.0;
    else
        fragColor = debugColor;

    gl_PointSize = 3.0;
    gl_Position = push_const.mvp * vec4(inPosition, 1.0);
}
