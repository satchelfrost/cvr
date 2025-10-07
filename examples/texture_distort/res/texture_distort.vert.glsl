#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 out_uv;

layout(binding = 0) uniform floats {
    int movement;
    int index;
    int tiles_per_side;
    int mode;
    int reset;
    float speed;
} ubo;

layout(std430, binding = 1) buffer depth_data {
   vec2 offsets[ ];
};

#define MOVEMENT_LEFT  1
#define MOVEMENT_RIGHT 2
#define MOVEMENT_DOWN  4
#define MOVEMENT_UP    8

#define MODE_CLEAR 2

void main()
{
    if ((ubo.movement & MOVEMENT_LEFT)  > 0) offsets[ubo.index].x -= ubo.speed;
    if ((ubo.movement & MOVEMENT_RIGHT) > 0) offsets[ubo.index].x += ubo.speed;
    if ((ubo.movement & MOVEMENT_DOWN)  > 0) offsets[ubo.index].y += ubo.speed;
    if ((ubo.movement & MOVEMENT_UP)    > 0) offsets[ubo.index].y -= ubo.speed;
    if (ubo.reset == 1) {
        offsets[gl_VertexIndex].x = 0.0f;
        offsets[gl_VertexIndex].y = 0.0f;
    }
    vec2 pos = position + offsets[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    out_uv = uv;
}
