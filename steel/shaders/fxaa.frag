#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D u_scene;

// FXAA 3.11 — Quality preset 12
// Quality steps: 1,1,1,1,1,1.5,2,2,2,2,4,8
const int SEARCH_STEPS = 12;
const float SEARCH_OFFSETS[12] = float[](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

const float EDGE_THRESHOLD     = 0.0312;
const float RELATIVE_THRESHOLD = 0.125;
const float SUBPIX_QUALITY     = 0.75;

float luminance(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 texel_size = 1.0 / vec2(textureSize(u_scene, 0));

    // ---- Sample center and 8 neighbors ----
    vec3 rgb_c = texture(u_scene, in_uv).rgb;
    float luma_c = luminance(rgb_c);

    float luma_n  = luminance(texture(u_scene, in_uv + vec2( 0.0, -1.0) * texel_size).rgb);
    float luma_s  = luminance(texture(u_scene, in_uv + vec2( 0.0,  1.0) * texel_size).rgb);
    float luma_w  = luminance(texture(u_scene, in_uv + vec2(-1.0,  0.0) * texel_size).rgb);
    float luma_e  = luminance(texture(u_scene, in_uv + vec2( 1.0,  0.0) * texel_size).rgb);
    float luma_nw = luminance(texture(u_scene, in_uv + vec2(-1.0, -1.0) * texel_size).rgb);
    float luma_ne = luminance(texture(u_scene, in_uv + vec2( 1.0, -1.0) * texel_size).rgb);
    float luma_sw = luminance(texture(u_scene, in_uv + vec2(-1.0,  1.0) * texel_size).rgb);
    float luma_se = luminance(texture(u_scene, in_uv + vec2( 1.0,  1.0) * texel_size).rgb);

    // ---- Early exit: no edge ----
    float luma_min = min(luma_c, min(min(luma_n, luma_s), min(luma_w, luma_e)));
    float luma_max = max(luma_c, max(max(luma_n, luma_s), max(luma_w, luma_e)));
    float luma_range = luma_max - luma_min;

    if (luma_range < max(EDGE_THRESHOLD, luma_max * RELATIVE_THRESHOLD)) {
        out_color = vec4(rgb_c, 1.0);
        return;
    }

    // ---- Sub-pixel aliasing factor ----
    float luma_avg = (luma_n + luma_s + luma_w + luma_e) * 0.25;
    float sub_pixel = clamp(abs(luma_avg - luma_c) / luma_range, 0.0, 1.0);
    float sub_pixel_blend = smoothstep(0.0, 1.0, sub_pixel);
    sub_pixel_blend = sub_pixel_blend * sub_pixel_blend * SUBPIX_QUALITY;

    // ---- Determine edge orientation ----
    float edge_h = abs(luma_nw + luma_ne - 2.0 * luma_n)
                 + abs(luma_w  + luma_e  - 2.0 * luma_c) * 2.0
                 + abs(luma_sw + luma_se - 2.0 * luma_s);

    float edge_v = abs(luma_nw + luma_sw - 2.0 * luma_w)
                 + abs(luma_n  + luma_s  - 2.0 * luma_c) * 2.0
                 + abs(luma_ne + luma_se - 2.0 * luma_e);

    bool is_horizontal = (edge_h >= edge_v);

    // Step size perpendicular to the edge (used for straddling samples)
    float step_length = is_horizontal ? texel_size.y : texel_size.x;

    // Pick the side with the steeper gradient
    float luma_pos = is_horizontal ? luma_s : luma_e;
    float luma_neg = is_horizontal ? luma_n : luma_w;
    float gradient_pos = abs(luma_pos - luma_c);
    float gradient_neg = abs(luma_neg - luma_c);

    bool pos_is_steeper = (gradient_pos >= gradient_neg);
    float gradient_scaled = 0.25 * max(gradient_pos, gradient_neg);

    // Luma at the edge (average of center and the neighbor on the steeper side)
    float luma_edge = pos_is_steeper
        ? 0.5 * (luma_c + luma_pos)
        : 0.5 * (luma_c + luma_neg);

    // Step direction perpendicular to edge (toward steeper side)
    if (!pos_is_steeper) {
        step_length = -step_length;
    }

    // ---- Edge endpoint search ----
    // Start at the edge center, offset half a texel perpendicular to the edge
    vec2 edge_uv = in_uv;
    if (is_horizontal) {
        edge_uv.y += step_length * 0.5;
    } else {
        edge_uv.x += step_length * 0.5;
    }

    // Search direction along the edge
    vec2 edge_step = is_horizontal ? vec2(texel_size.x, 0.0) : vec2(0.0, texel_size.y);

    // Walk in both directions along the edge
    vec2 uv_pos = edge_uv + edge_step * SEARCH_OFFSETS[0];
    vec2 uv_neg = edge_uv - edge_step * SEARCH_OFFSETS[0];

    float luma_end_pos = luminance(texture(u_scene, uv_pos).rgb) - luma_edge;
    float luma_end_neg = luminance(texture(u_scene, uv_neg).rgb) - luma_edge;

    bool reached_pos = abs(luma_end_pos) >= gradient_scaled;
    bool reached_neg = abs(luma_end_neg) >= gradient_scaled;
    bool reached_both = reached_pos && reached_neg;

    // Continue searching until both endpoints found
    for (int i = 1; i < SEARCH_STEPS && !reached_both; i++) {
        if (!reached_pos) {
            uv_pos += edge_step * SEARCH_OFFSETS[i];
            luma_end_pos = luminance(texture(u_scene, uv_pos).rgb) - luma_edge;
            reached_pos = abs(luma_end_pos) >= gradient_scaled;
        }
        if (!reached_neg) {
            uv_neg -= edge_step * SEARCH_OFFSETS[i];
            luma_end_neg = luminance(texture(u_scene, uv_neg).rgb) - luma_edge;
            reached_neg = abs(luma_end_neg) >= gradient_scaled;
        }
        reached_both = reached_pos && reached_neg;
    }

    // ---- Compute edge blend factor ----
    // Distance to each endpoint
    float dist_pos, dist_neg;
    if (is_horizontal) {
        dist_pos = uv_pos.x - in_uv.x;
        dist_neg = in_uv.x - uv_neg.x;
    } else {
        dist_pos = uv_pos.y - in_uv.y;
        dist_neg = in_uv.y - uv_neg.y;
    }

    // Which endpoint is closer?
    bool closer_is_neg = (dist_neg < dist_pos);
    float dist_closest = min(dist_pos, dist_neg);
    float edge_length = dist_pos + dist_neg;

    // Check if the luma variation at the closer end is in the same direction
    // as the luma variation at the center. If not, this pixel is on the wrong
    // side of the edge and shouldn't be blended.
    float luma_end_closest = closer_is_neg ? luma_end_neg : luma_end_pos;
    bool good_span = ((luma_c - luma_edge) < 0.0) != (luma_end_closest < 0.0);

    // Pixel blend: 0.5 - (distance / edge_length), clamped to [0, 0.5]
    float edge_blend = good_span ? (0.5 - dist_closest / edge_length) : 0.0;

    // ---- Final blend ----
    float final_blend = max(edge_blend, sub_pixel_blend);

    // Sample at the blended UV (offset perpendicular to the edge)
    vec2 final_uv = in_uv;
    if (is_horizontal) {
        final_uv.y += step_length * final_blend;
    } else {
        final_uv.x += step_length * final_blend;
    }

    out_color = vec4(texture(u_scene, final_uv).rgb, 1.0);
}
