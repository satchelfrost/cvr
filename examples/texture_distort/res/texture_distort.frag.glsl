#version 450

#define DARKGRAY   vec4(  80/255.0f,  80/255.0f,  80/255.0f, 1.0f)
#define RED        vec4( 230/255.0f,  41/255.0f,  55/255.0f, 1.0f)
#define SKYBLUE    vec4( 102/255.0f, 191/255.0f, 255/255.0f, 1.0f)

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform ubo_data {
    int keypress;
    int index;
    int tiles_per_side;
    int mode;
    int reset;
    float speed;
} ubo;

layout(std430, binding = 1) buffer depth_data {
   vec2 offsets[ ];
};

layout(binding = 2) uniform sampler2D tex_sampler;

void main()
{
    // pretend the quad has 100 pixels
    ivec2 virtual_pixel_coord = ivec2(uv*vec2(100));

    if (ubo.mode == 1) {
        out_color = texture(tex_sampler, uv);
    } else {
        // checkerboard
        int divisor = int(ceil(100.0f/float(ubo.tiles_per_side)));
        int a = (virtual_pixel_coord.x/divisor) % 2;
        int b = (virtual_pixel_coord.y/divisor) % 2;
        out_color = ((a+b)%2 == 0) ? SKYBLUE : DARKGRAY;
    }

    // place a red circle over the current vertex we are moving
    int verts_per_side = ubo.tiles_per_side + 1;
    float x = (ubo.index % verts_per_side)/float(ubo.tiles_per_side);
    float y = (int(ubo.index/verts_per_side) % verts_per_side)/float(ubo.tiles_per_side);
    float l = length(uv - vec2(x, y));
    if (l < 0.03) out_color = RED;
}
