#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float time;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(((fragColor) * ((sin(time) + 1.0) / 2.0)) + texture(texSampler, fragTexCoord).rgb * ((sin(time + 3.14159) + 1.0) / 2.0), 1.0);
}
