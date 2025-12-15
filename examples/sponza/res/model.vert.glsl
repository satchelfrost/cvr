#version 450

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec4 in_tanget;

layout (binding = 0) uniform UBOScene {
    mat4 proj;
    mat4 view;
    vec4 light_pos;
    vec4 view_pos;
} ubo;

layout(push_constant) uniform PushConsts {
    mat4 model;
    vec4 base_color_factor;
} primitive;

layout (location = 0) out vec2 out_uv;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec4 out_tanget;
layout (location = 3) out vec3 out_view_vec;
layout (location = 4) out vec3 out_light_vec;

void main()
{
    out_uv     = in_uv;
    out_tanget = in_tanget;

    gl_Position   = ubo.proj * ubo.view * primitive.model * vec4(in_pos, 1.0);

    out_normal    = mat3(primitive.model) * in_normal;
    vec4 pos      = primitive.model * vec4(in_pos, 1.0);
    out_view_vec  = ubo.view.pos.xyz - pos;
    out_light_vec = ubo.light.pos.xyz - pos;
}
