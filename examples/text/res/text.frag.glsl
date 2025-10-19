#version 450

layout (binding = 0) uniform sampler2D tex_sampler;
layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

void main()
{
    float alpha = texture(tex_sampler, in_uv).r;
    vec4 text_color = vec4(0.0, 0.0, 0.0, 1.0);
    out_color = vec4(text_color.rgb, text_color.a*alpha);

    // out_color = texture(tex_sampler, in_uv);
}
