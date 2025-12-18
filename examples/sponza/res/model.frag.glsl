#version 450

layout (set = 1, binding = 0) uniform sampler2D sampler_color;
layout (set = 1, binding = 1) uniform sampler2D sampler_normal;

layout(push_constant) uniform PushConsts {
    mat4 model;
    vec4 base_color_factor;
} primitive;

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec4 in_tanget;
layout (location = 3) in vec3 in_view_vec;
layout (location = 4) in vec3 in_light_vec;

layout (location = 0) out vec4 out_color;

#define AMBIENT 0.1

void main()
{
    vec3 n   = normalize(in_normal);
    vec3 t   = normalize(in_tanget.xyz);
    vec3 b   = cross(in_normal, in_tanget.xyz) * in_tanget.w;
    mat3 tbn = mat3(t, b, n);
    vec3 N   = tbn * normalize(texture(sampler_normal, in_uv).xyz * 2.0 - vec3(1.0));

    vec3 l = normalize(in_light_vec);
    vec3 v = normalize(in_view_vec);
    vec3 r = reflect(-l, N);
    float diffuse  = max(dot(N, l), AMBIENT);
    float specular = pow(max(dot(r, v), 0.0), 32.0);

    out_color = texture(sampler_color, in_uv) * primitive.base_color_factor;
    out_color.rgb  = diffuse*out_color.rgb + specular*out_color.rgb;
    // out_color = vec4(n, 1.0);
}
