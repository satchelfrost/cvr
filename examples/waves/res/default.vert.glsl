#version 450

layout(push_constant) uniform constants
{
    mat4 mvp;
} push_const;


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 fragColor;

void main()
{
    gl_Position = push_const.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
}
