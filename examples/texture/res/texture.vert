#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;

layout(binding = 0) uniform UniformBufferObject
{
    float time;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUv;
layout(location = 2) out float time;

void main()
{
    gl_Position = push_const.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragUv = inUv;
    time = ubo.time;
}
