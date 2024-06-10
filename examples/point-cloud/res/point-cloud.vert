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
    mat4 mvp4;
} cameras;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uvec3 inColor;

layout(location = 0) out vec3 fragColor;

void main()
{
    gl_PointSize = 3.0;
    gl_Position = push_const.mvp * vec4(inPosition, 1.0);
    fragColor = vec3(inColor) / 255.0;
}
