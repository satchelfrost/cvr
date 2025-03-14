#version 450

#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable

#define NUM_CCTVS 4
#define NUM_VIDEO_PLANES 3
#define NEAR 0.01
#define FAR 1000.0

vec4 tex_colors[NUM_CCTVS];
int cam_sees[NUM_CCTVS];
vec4 cam_clip[NUM_CCTVS];
float distances[NUM_CCTVS];
float seam_blend_ratios[NUM_CCTVS];
bool in_shadow[NUM_CCTVS];

struct Camera {
    float vtx_dot;
    int cam_id;
};
Camera cams_of_interest[NUM_CCTVS];

layout(push_constant) uniform constants
{
    uint offset;
    uint point_count;
} push_const;

struct Vertex {
    float x, y, z;
    uint color;
};

/* shader modes described in more detail in log_shader_mode() in main.c */
#define MODE_MODEL               0
#define MODE_FRUSTUM_OVERLAP     1
#define MODE_SINGLE_VID_TEX      2
#define MODE_SEAM_AND_VIEW_BLEND 3
#define MODE_WIPE                4
#define MODE_ANGLE_BLEND         5

layout(binding = 0) uniform uniform_data {
    mat4 mvp;                 // main viewing camera's view-proj w/ model
    mat4 mvps[NUM_CCTVS];     // cctv camera's view-proj w/ model
    vec4 cctv_pos[NUM_CCTVS]; // cctv position in world space
    vec4 main_cam_pos;
    mat4 model;               // model matrix in world space
    int shader_mode;
    int cam_order_0;          // index of closest cctv to main viewing camera
    int cam_order_1;
    int cam_order_2;
    int cam_order_3;          // index of furthest cctv from main viewing camera
    float blend_ratio;        // blend ratio for two closest cctvs to main viewing camera
    int frame_width;
    int frame_height;
    int elevation_based_occlusion;
    vec2 img_sizes[NUM_CCTVS]; 
} ubo;

layout(std430, binding = 1) buffer vert_data {
   Vertex vertices[ ];
};

layout(std430, binding = 2) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(std430, binding = 3) buffer depth_data {
   uint depth_buffs[ ];
};

/* uses binding 4 -> 15 */
layout(binding = 4) uniform sampler2D samplers[NUM_CCTVS * NUM_VIDEO_PLANES];

mat4 rec601 = mat4(
    1.16438,  0.00000,  1.59603, -0.87079,
    1.16438, -0.39176, -0.81297,  0.52959,
    1.16438,  2.01723,  0.00000, -1.08139,
    0, 0, 0, 1
);

layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

uint vec3_to_uint(vec3 color)
{
    color = min(color * 255, 255);
    return uint(color.b) << 16 | uint(color.g) << 8 | uint(color.r);
}

float linearize_depth(float depth)
{
    float n = NEAR;
    float f = FAR;
    float z = depth;
    return (2.0 * n) / (f + n - z * (f - n));
}

void main()
{
    uint index = gl_GlobalInvocationID.x + push_const.offset;
    if (index > push_const.point_count) return;
    Vertex vert = vertices[index];
    vec4 pos = ubo.mvp * vec4(vert.x, vert.y, vert.z, 1.0);
    vec3 ndc = pos.xyz / pos.w;

    if (pos.w <= 0 || ndc.x < -1.0 || ndc.x > 1.0 || ndc.y < -1.0 || ndc.y > 1.0)
        return;

    /* fake occlusion by limiting the height to a little under the stadium floor */
    // int shader_mode = (ubo.elevation_based_occlusion == 1 && vert.z < -1.5) ? MODE_MODEL : ubo.shader_mode;
    int shader_mode = ubo.shader_mode;

    /* don't incur the cost of texture sampling if we are only interested in the colors from the model */
    if (shader_mode != MODE_MODEL) {
        int offset = 0;
        for (int i = 0; i < NUM_CCTVS; i++) {
            /* convert the vertex into each cctvs clip coordinate space */
            vec4 cam_clip = ubo.mvps[i] * vec4(vert.x, vert.y, vert.z, 1.0);
            vec3 ndc = cam_clip.xyz / cam_clip.w;
            vec2 uv = (ndc.xy * 0.5) + 0.5;

            /* calculate the final texture from the three video planes */
            float y  = textureLod(samplers[0 + i * NUM_VIDEO_PLANES], uv, 0).r;
            float cb = textureLod(samplers[1 + i * NUM_VIDEO_PLANES], uv, 0).r;
            float cr = textureLod(samplers[2 + i * NUM_VIDEO_PLANES], uv, 0).r;
            tex_colors[i] = vec4(y, cb, cr, 1.0) * rec601;

            /* determine if vertex can be seen by a particular cctv */
            cam_sees[i] = int((-cam_clip.w < cam_clip.x && cam_clip.x < cam_clip.w) &&
                              (-cam_clip.w < cam_clip.y && cam_clip.y < cam_clip.w) &&
                              (-cam_clip.w < cam_clip.z && cam_clip.z < cam_clip.w));

            /* occlusion check */
            ivec2 pixel_coords = ivec2(uv * vec2(1280.0, 960.0));
            ivec2 img_size = ivec2(1280, 960);
            int pixel_id = pixel_coords.x + pixel_coords.y * img_size.x;
            float depth = uintBitsToFloat(depth_buffs[pixel_id]);
            in_shadow[i] = (depth + 0.5 < cam_clip.w) ? true : false;

            // TODO: possibly move this outside of the loop
            /* calculate the distances from the vertex to the cctvs in world space */
            distances[i] = distance(ubo.model * vec4(vert.x, vert.y, vert.z, 1.0), ubo.cctv_pos[i]);

            seam_blend_ratios[i] = 1.0;
            if (uv.x < 0.05) seam_blend_ratios[i] = smoothstep(0.0, 0.05, uv.x);
            if (uv.x > 0.95) seam_blend_ratios[i] = smoothstep(1.0, 0.95, uv.x);
            if (uv.y < 0.05) {
                if (uv.x < 0.05 || uv.x > 0.95) {
                    seam_blend_ratios[i] = min(smoothstep(0.0, 0.05, uv.y), seam_blend_ratios[i]);
                } else {
                    seam_blend_ratios[i] = smoothstep(0.0, 0.05, uv.y);
                }
            }
            if (uv.y > 0.95) {
                if (uv.x > 0.95 || uv.x < 0.05) {
                    seam_blend_ratios[i] = min(smoothstep(1.0, 0.95, uv.y), seam_blend_ratios[i]);
                } else {
                    seam_blend_ratios[i] = smoothstep(1.0, 0.95, uv.y);
                }
            }
        }
    }

    uint brightness = uint((cam_sees[0] * 0.25 + cam_sees[1] * 0.25 + cam_sees[2] * 0.25 + cam_sees[3] * 0.25) * 255.0);
    uint out_color = vert.color;
    vec4 world_vert = ubo.model * vec4(vert.x, vert.y, vert.z, 1.0);
    vec4 to_cam     = normalize(ubo.main_cam_pos - world_vert);
    int cam_see_count = 0;
    vec4 temp_mix;
    float ratio_color;
    float dist_blend_ratio;
    switch (shader_mode) {
    case MODE_MODEL:
        break;
    case MODE_FRUSTUM_OVERLAP:
        out_color = brightness << 16 | brightness << 8 | brightness;
        break;
    case MODE_SINGLE_VID_TEX:
        // out_color = (cam_sees[ubo.cam_order_0] > 0 && !in_shadow[ubo.cam_order_0]) ? vec3_to_uint(tex_colors[ubo.cam_order_0].rgb) : vert.color;
        out_color = (cam_sees[ubo.cam_order_0] > 0) ? vec3_to_uint(tex_colors[ubo.cam_order_0].rgb) : vert.color;
        if (in_shadow[ubo.cam_order_0]) out_color = vert.color;
        break;
    case MODE_SEAM_AND_VIEW_BLEND:
        if (cam_sees[ubo.cam_order_3] > 0) out_color = vec3_to_uint(tex_colors[ubo.cam_order_3].rgb);

        if (cam_sees[ubo.cam_order_2] > 0) {
            if (cam_sees[ubo.cam_order_3] > 0) {
                temp_mix = mix(tex_colors[ubo.cam_order_3], tex_colors[ubo.cam_order_2], seam_blend_ratios[ubo.cam_order_2]);
                out_color = vec3_to_uint(temp_mix.rgb);
            } else {
                out_color = vec3_to_uint(tex_colors[ubo.cam_order_2].rgb);
            }
        }

        if (cam_sees[ubo.cam_order_1] > 0) {
            if (cam_sees[ubo.cam_order_2] > 0) {
                temp_mix = mix(tex_colors[ubo.cam_order_2], tex_colors[ubo.cam_order_1], seam_blend_ratios[ubo.cam_order_1]);
                out_color = vec3_to_uint(temp_mix.rgb);
            } else if (cam_sees[ubo.cam_order_3] > 0) {
                temp_mix = mix(tex_colors[ubo.cam_order_3], tex_colors[ubo.cam_order_1], seam_blend_ratios[ubo.cam_order_1]);
                out_color = vec3_to_uint(temp_mix.rgb);
            } else {
                out_color = vec3_to_uint(tex_colors[ubo.cam_order_1].rgb);
            }
        }

        if (cam_sees[ubo.cam_order_0] > 0) {
            if (cam_sees[ubo.cam_order_1] > 0) {
                dist_blend_ratio = 1.0 - smoothstep(0.35, 0.65, ubo.blend_ratio);
                if (seam_blend_ratios[ubo.cam_order_1] < 1.0) {
                    temp_mix = mix(tex_colors[ubo.cam_order_1], tex_colors[ubo.cam_order_0], max(dist_blend_ratio, (1.0 - seam_blend_ratios[ubo.cam_order_1])));
                } else {
                    temp_mix = mix(tex_colors[ubo.cam_order_1], tex_colors[ubo.cam_order_0], min(dist_blend_ratio , seam_blend_ratios[ubo.cam_order_0]));
                }
                out_color = vec3_to_uint(temp_mix.rgb);
            } else if (cam_sees[ubo.cam_order_2] > 0) {
                temp_mix = mix(tex_colors[ubo.cam_order_2], tex_colors[ubo.cam_order_0], seam_blend_ratios[ubo.cam_order_0]);
                out_color = vec3_to_uint(temp_mix.rgb);
            } else if (cam_sees[ubo.cam_order_3] > 0) {
                temp_mix = mix(tex_colors[ubo.cam_order_3], tex_colors[ubo.cam_order_0], seam_blend_ratios[ubo.cam_order_0]);
                out_color = vec3_to_uint(temp_mix.rgb);
            } else {
                out_color = vec3_to_uint(tex_colors[ubo.cam_order_0].rgb);
            }
        }
        break;
    case MODE_WIPE:
        cams_of_interest[0].vtx_dot = dot(normalize(ubo.cctv_pos[0] - world_vert), to_cam);
        cams_of_interest[1].vtx_dot = dot(normalize(ubo.cctv_pos[1] - world_vert), to_cam);
        cams_of_interest[2].vtx_dot = dot(normalize(ubo.cctv_pos[2] - world_vert), to_cam);
        cams_of_interest[3].vtx_dot = dot(normalize(ubo.cctv_pos[3] - world_vert), to_cam);
        cams_of_interest[0].cam_id = 0;
        cams_of_interest[1].cam_id = 1;
        cams_of_interest[2].cam_id = 2;
        cams_of_interest[3].cam_id = 3;
        /* leave cameras of interest in list if they can see the vertex */
        for (int i = 0; i < NUM_CCTVS; i++) {
            if (cam_sees[i] > 0) {
                cams_of_interest[cam_see_count] = cams_of_interest[i];
                cam_see_count++;
            }
        }
        if (cam_see_count == 0) break; // base point cloud color
        /* sort the cameras based on the highest dot product */
        for (int i = 0; i < cam_see_count; i++) {
            for (int j = i + 1; j < cam_see_count; j++) {
                if (cams_of_interest[i].vtx_dot < cams_of_interest[j].vtx_dot) {
                    Camera tmp = cams_of_interest[i];
                    cams_of_interest[i] = cams_of_interest[j];
                    cams_of_interest[j] = tmp;
                }
            }
        }

        out_color = vec3_to_uint(tex_colors[cams_of_interest[0].cam_id].rgb);
        break;
    case MODE_ANGLE_BLEND:
        cams_of_interest[0].vtx_dot = dot(normalize(ubo.cctv_pos[0] - world_vert), to_cam);
        cams_of_interest[1].vtx_dot = dot(normalize(ubo.cctv_pos[1] - world_vert), to_cam);
        cams_of_interest[2].vtx_dot = dot(normalize(ubo.cctv_pos[2] - world_vert), to_cam);
        cams_of_interest[3].vtx_dot = dot(normalize(ubo.cctv_pos[3] - world_vert), to_cam);
        cams_of_interest[0].cam_id = 0;
        cams_of_interest[1].cam_id = 1;
        cams_of_interest[2].cam_id = 2;
        cams_of_interest[3].cam_id = 3;
        /* leave cameras of interest in list if they can see the vertex */
        for (int i = 0; i < NUM_CCTVS; i++) {
            if (cam_sees[i] > 0) {
                cams_of_interest[cam_see_count] = cams_of_interest[i];
                cam_see_count++;
            }
        }
        if (cam_see_count == 0) break; // base point cloud color
        /* sort the cameras based on the highest dot product */
        for (int i = 0; i < cam_see_count; i++) {
            for (int j = i + 1; j < cam_see_count; j++) {
                if (cams_of_interest[i].vtx_dot < cams_of_interest[j].vtx_dot) {
                    Camera tmp = cams_of_interest[i];
                    cams_of_interest[i] = cams_of_interest[j];
                    cams_of_interest[j] = tmp;
                }
            }
        }

        if (cam_see_count == 1)
            out_color = vec3_to_uint(tex_colors[cams_of_interest[0].cam_id].rgb);
        else {
            float oj = acos(cams_of_interest[0].vtx_dot);
            float ok = acos(cams_of_interest[1].vtx_dot);
            float b = oj / (oj + ok);
            float a = ok / (oj + ok);
            vec4 final = a * tex_colors[cams_of_interest[0].cam_id] + b * tex_colors[cams_of_interest[1].cam_id];
            out_color = vec3_to_uint(final.rgb);
        }
        break;
    }

    /* determine the pixel coordinate and write to framebuffer */
    ivec2 img_size = ivec2(ubo.frame_width, ubo.frame_height);
    vec2 img_pos = (ndc.xy * 0.5 + 0.5) * img_size;
    ivec2 pixel_coords = ivec2(img_pos);
    int pixel_id = pixel_coords.x + pixel_coords.y * img_size.x;
    uint64_t depth = floatBitsToUint(pos.w);
    uint64_t old_depth = frame_buff[pixel_id] >> 32;

    if (depth < old_depth) {
        uint64_t packed = depth << 32 | uint64_t(out_color);
        atomicMin(frame_buff[pixel_id], packed);
    }
}
