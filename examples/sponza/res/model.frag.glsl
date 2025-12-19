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

vec4 srgb_to_linear(vec4 srgbIn)
{
	// vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	return vec4(linOut,srgbIn.w);;
}

void main()
{
    // vec3 n   = normalize(in_normal);
    // vec3 t   = normalize(in_tanget.xyz);
    // vec3 b   = cross(in_normal, in_tanget.xyz) * in_tanget.w;
    // mat3 tbn = mat3(t, b, n);
    // vec3 N   = normalize(tbn * texture(sampler_normal, in_uv).xyz * 2.0 - vec3(1.0));

    vec3 n            = normalize(in_normal);
    vec3 t            = normalize(in_tanget.xyz);
    vec3 b            = cross(n, t) * in_tanget.w;
    mat3 TBN          = mat3(t, b, n);
    vec3 local_normal = texture(sampler_normal, in_uv).xyz * 2.0 - 1.0;
    vec3 N            = normalize(TBN * local_normal);
    // N                 = n;

    vec3 l = normalize(in_light_vec);
    vec3 v = normalize(in_view_vec);
    vec3 r = reflect(-l, N);

    float d = dot(N, l);
    // float diffuse  = clamp(smoothstep(0.45, 0.55, d), AMBIENT, 1.0);
    float diffuse  = max(dot(N, l), AMBIENT);

    float specular = pow(max(dot(r, v), 0.0), 32.0);

    vec4 color = srgb_to_linear(texture(sampler_color, in_uv));
    out_color = color * primitive.base_color_factor;
    out_color.rgb  = diffuse*out_color.rgb + specular*out_color.rgb;
    // out_color.rgb  = diffuse*out_color.rgb;

    // N = 0.5*N + 0.5;
    // out_color = vec4(N, 1.0);
}
