#version 450

#define DARKGRAY   vec4(  80/255.0f,  80/255.0f,  80/255.0f, 1.0f)
#define RED        vec4( 230/255.0f,  41/255.0f,  55/255.0f, 1.0f)
#define SKYBLUE    vec4( 102/255.0f, 191/255.0f, 255/255.0f, 1.0f)

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 color;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform ubo_data {
    int keypress;
    int index;
    float speed;
} ubo;

layout(std430, binding = 1) buffer depth_data {
   vec2 offsets[ ];
};

void main()
{
    // pretend the quad has 100 pixels
    ivec2 virtual_pixel_coord = ivec2(uv*vec2(100));

    // every 25 virtual pixels swap which color for checkerboard
    int a = (virtual_pixel_coord.x/25) % 2;
    int b = (virtual_pixel_coord.y/25) % 2;
    out_color = ((a+b)%2 == 0) ? SKYBLUE : DARKGRAY;

    // place a red circle over the current vertex we are moving
    float x = 0.0f;
    float y = 0.0f;
    switch (ubo.index) {
        case 0: x = 0.0; y = 0.0; break;
        case 1: x = 1.0; y = 0.0; break;
        case 2: x = 0.0; y = 1.0; break;
        case 3: x = 1.0; y = 1.0; break;
    }
    float l = length(uv - vec2(x, y));
    if (l < 0.05) out_color = RED;
}
