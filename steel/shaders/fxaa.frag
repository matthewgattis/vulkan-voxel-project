#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D u_scene;

void main() {
    vec2 texel_size = 1.0 / vec2(textureSize(u_scene, 0));

    // Sample center and 4 cardinal neighbors
    vec3 rgb_c  = texture(u_scene, in_uv).rgb;
    vec3 rgb_n  = texture(u_scene, in_uv + vec2( 0.0, -1.0) * texel_size).rgb;
    vec3 rgb_s  = texture(u_scene, in_uv + vec2( 0.0,  1.0) * texel_size).rgb;
    vec3 rgb_w  = texture(u_scene, in_uv + vec2(-1.0,  0.0) * texel_size).rgb;
    vec3 rgb_e  = texture(u_scene, in_uv + vec2( 1.0,  0.0) * texel_size).rgb;

    // Diagonal neighbors
    vec3 rgb_nw = texture(u_scene, in_uv + vec2(-1.0, -1.0) * texel_size).rgb;
    vec3 rgb_ne = texture(u_scene, in_uv + vec2( 1.0, -1.0) * texel_size).rgb;
    vec3 rgb_sw = texture(u_scene, in_uv + vec2(-1.0,  1.0) * texel_size).rgb;
    vec3 rgb_se = texture(u_scene, in_uv + vec2( 1.0,  1.0) * texel_size).rgb;

    // Luminance (perceptual weights)
    const vec3 luma_weights = vec3(0.299, 0.587, 0.114);
    float luma_c  = dot(rgb_c,  luma_weights);
    float luma_n  = dot(rgb_n,  luma_weights);
    float luma_s  = dot(rgb_s,  luma_weights);
    float luma_w  = dot(rgb_w,  luma_weights);
    float luma_e  = dot(rgb_e,  luma_weights);
    float luma_nw = dot(rgb_nw, luma_weights);
    float luma_ne = dot(rgb_ne, luma_weights);
    float luma_sw = dot(rgb_sw, luma_weights);
    float luma_se = dot(rgb_se, luma_weights);

    // Edge detection
    float luma_min = min(luma_c, min(min(luma_n, luma_s), min(luma_w, luma_e)));
    float luma_max = max(luma_c, max(max(luma_n, luma_s), max(luma_w, luma_e)));
    float luma_range = luma_max - luma_min;

    // FXAA 3.11 quality preset 12 thresholds
    const float edge_threshold     = 0.0312;
    const float relative_threshold = 0.125;

    if (luma_range < max(edge_threshold, luma_max * relative_threshold)) {
        out_color = vec4(rgb_c, 1.0);
        return;
    }

    // Determine edge direction
    float edge_h = abs(luma_nw + luma_ne - 2.0 * luma_n)
                 + abs(luma_w  + luma_e  - 2.0 * luma_c) * 2.0
                 + abs(luma_sw + luma_se - 2.0 * luma_s);

    float edge_v = abs(luma_nw + luma_sw - 2.0 * luma_w)
                 + abs(luma_n  + luma_s  - 2.0 * luma_c) * 2.0
                 + abs(luma_ne + luma_se - 2.0 * luma_e);

    bool is_horizontal = (edge_h >= edge_v);

    // Choose blend direction perpendicular to edge
    float luma_pos = is_horizontal ? luma_s : luma_e;
    float luma_neg = is_horizontal ? luma_n : luma_w;

    float gradient_pos = abs(luma_pos - luma_c);
    float gradient_neg = abs(luma_neg - luma_c);

    vec2 step_dir;
    if (is_horizontal) {
        step_dir = (gradient_pos >= gradient_neg)
            ? vec2(0.0,  texel_size.y)
            : vec2(0.0, -texel_size.y);
    } else {
        step_dir = (gradient_pos >= gradient_neg)
            ? vec2( texel_size.x, 0.0)
            : vec2(-texel_size.x, 0.0);
    }

    // Sub-pixel anti-aliasing factor
    float luma_avg = (luma_n + luma_s + luma_w + luma_e) * 0.25;
    float sub_pixel = clamp(abs(luma_avg - luma_c) / luma_range, 0.0, 1.0);
    float sub_pixel_blend = smoothstep(0.0, 1.0, sub_pixel);
    sub_pixel_blend = sub_pixel_blend * sub_pixel_blend * 0.75;

    // Blend along edge direction
    vec2 blend_uv = in_uv + step_dir * 0.5;
    vec3 result = texture(u_scene, blend_uv).rgb;

    // Apply sub-pixel blending
    float blend_factor = max(sub_pixel_blend, 0.0);
    result = mix(rgb_c, result, blend_factor);

    out_color = vec4(result, 1.0);
}
