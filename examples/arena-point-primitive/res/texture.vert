#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 cammeraMvp1;
    mat4 cammeraMvp2;
    mat4 cammeraMvp3;
    mat4 cammeraMvp4;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUv;

void main()
{
    gl_Position = push_const.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragUv = inUv;
}
