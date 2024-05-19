#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    float dist = length(2 * gl_PointCoord - 1.0);
    if (dist > 1.0)
        discard;
    outColor = vec4(fragColor, 1.0);
}
