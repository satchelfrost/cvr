#version 450

layout (binding = 0) uniform sampler2D tex_sampler;
layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

layout(push_constant) uniform constants
{
    int x;
    int y;
    int x0;
    int y0;
    int x1;
    int y1;
    float xoffset;
    float yoffset;
} push_const;

float lerp(float a, float b, float v)
{
    return (b - a)*v + a;
}

void main()
{
    // R
    // (x0,y0)  (x1,y0)
    //    +-----+
    //    -------
    //    -------
    //    -------
    //    +-----+
    // (x0,y1)  (x1,y1)
    // int x0 = 354, y0 = 90, x1 = 379, y1 = 125;
    int x0 = push_const.x0, y0 = push_const.y0, x1 = push_const.x1, y1 = push_const.y1;
    // int x0 = 26, y0 = 181, x1 = 51, y1 = 209;
    // int x = x0 + 2;
    // int y = y0;
    // int x = 2;
    // int y = 27;
    int x = push_const.x;
    int y = push_const.y;
    // int x = 0;
    // int y = 37;

    ivec2 pixel_coords = ivec2(in_uv*vec2(400, 400));
    int dx = x1 - x0;
    int dy = y1 - y0;
    // int x_fin = pixel_coords.x + 

    if (pixel_coords.x >= x &&
        pixel_coords.x < x + dx &&
        pixel_coords.y >= y &&
        pixel_coords.y < y + dy) {
        // ivec2 tex = pixel_coords - ivec2(x, y) + ivec2(x0, y0);
        ivec2 tex = pixel_coords - ivec2(x, y) + ivec2(x0, y0);
        float alpha = texelFetch(tex_sampler, tex, 0).r;
        vec4 text_color = vec4(0.0, 0.0, 0.0, 1.0);
        out_color = vec4(text_color.rgb, text_color.a*alpha);
    }
    // if (pixel_coords.x > x0 &&
    //            pixel_coords.x < x1 &&
    //            pixel_coords.y > y0 &&
    //            pixel_coords.y < y1 ) {
    //     vec4 text_color = vec4(0.0, 0.0, 0.0, 1.0);
    //     // float alpha = texture(tex_sampler, in_uv).r;
    //     float alpha = texelFetch(tex_sampler, pixel_coords, 0).r;
    //     out_color = vec4(text_color.rgb, text_color.a*alpha);
    // }
    else {
        vec4 text_color = vec4(0.0, 0.0, 0.0, 1.0);
        out_color = vec4(text_color.rgb, 0);
        // out_color = texture(tex_sampler, in_uv);
    }

    // out_color = texture(tex_sampler, in_uv);

}
