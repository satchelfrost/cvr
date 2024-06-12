#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 ndc;
layout(location = 2) in vec3 debugColor;

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main()
{
    float dist = length(2 * gl_PointCoord - 1.0);
    if (dist > 1.0)
        discard;

    vec2 uv  = (ndc.xy * 0.5) + 0.5;

    if (0.0 < uv.x && uv.x < 1.0 && 0.0 < uv.y && uv.y < 1.0 && debugColor.x > 0.0)
        outColor = textureLod(texSampler, uv, 0.0);
    else
        outColor = vec4(fragColor, 1.0);
}
