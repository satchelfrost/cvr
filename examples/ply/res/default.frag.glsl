#version 450

layout(location = 0) in vec3 in_color;
layout(location = 0) out vec4 out_color;

vec4 srgb_to_linear(vec4 srgbIn)
{
	// vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );

	return vec4(linOut,srgbIn.w);;
}

void main()
{
    out_color = srgb_to_linear(vec4(in_color, 1.0));
}
