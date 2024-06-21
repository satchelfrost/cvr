#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

layout(binding = 0) uniform CameraMVPs
{
    mat4 mvps[4];
    int idx;
} cameras;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uvec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 ndc;
layout(location = 2) out vec3 debugColor;

void main()
{

    vec4 cam_rel_pos = cameras.mvps[cameras.idx] * vec4(inPosition, 1.0);
    ndc = cam_rel_pos.xyz / cam_rel_pos.w;
    debugColor = vec3(0.0, 0.0, 0.0);
    if ((-cam_rel_pos.w < cam_rel_pos.x && cam_rel_pos.x < cam_rel_pos.w) &&
        (-cam_rel_pos.w < cam_rel_pos.y && cam_rel_pos.y < cam_rel_pos.w) &&
        (-cam_rel_pos.w < cam_rel_pos.z && cam_rel_pos.z < cam_rel_pos.w)) {
        debugColor.x = 1.0;
    }
    if (debugColor == vec3(0.0, 0.0, 0.0))
        fragColor = vec3(inColor) / 255.0;
    else
        fragColor = debugColor;

    // vec4 cam1_rel_pos = cameras.mvps[0] * vec4(inPosition, 1.0);
    // vec4 cam2_rel_pos = cameras.mvps[1] * vec4(inPosition, 1.0);
    // vec4 cam3_rel_pos = cameras.mvps[2] * vec4(inPosition, 1.0);
    // vec4 cam4_rel_pos = cameras.mvps[3] * vec4(inPosition, 1.0);
    // ndc = cam1_rel_pos.xyz / cam1_rel_pos.w;

    // debugColor = vec3(0.0, 0.0, 0.0);
    // if ((-cam1_rel_pos.w < cam1_rel_pos.x && cam1_rel_pos.x < cam1_rel_pos.w) &&
    //     (-cam1_rel_pos.w < cam1_rel_pos.y && cam1_rel_pos.y < cam1_rel_pos.w) &&
    //     (-cam1_rel_pos.w < cam1_rel_pos.z && cam1_rel_pos.z < cam1_rel_pos.w)) {
    //     debugColor.x = 1.0;
    // }
    // if ((-cam2_rel_pos.w < cam2_rel_pos.x && cam2_rel_pos.x < cam2_rel_pos.w) &&
    //     (-cam2_rel_pos.w < cam2_rel_pos.y && cam2_rel_pos.y < cam2_rel_pos.w) &&
    //     (-cam2_rel_pos.w < cam2_rel_pos.z && cam2_rel_pos.z < cam2_rel_pos.w)) {
    //     debugColor.y = 1.0;
    // }
    // if ((-cam3_rel_pos.w < cam3_rel_pos.x && cam3_rel_pos.x < cam3_rel_pos.w) &&
    //     (-cam3_rel_pos.w < cam3_rel_pos.y && cam3_rel_pos.y < cam3_rel_pos.w) &&
    //     (-cam3_rel_pos.w < cam3_rel_pos.z && cam3_rel_pos.z < cam3_rel_pos.w)) {
    //     debugColor.z = 1.0;
    // }

    // if (debugColor == vec3(0.0, 0.0, 0.0))
    //     fragColor = vec3(inColor) / 255.0;
    // else
    //     fragColor = debugColor;

    gl_PointSize = 3.0;
    gl_Position = push_const.mvp * vec4(inPosition, 1.0);
}
