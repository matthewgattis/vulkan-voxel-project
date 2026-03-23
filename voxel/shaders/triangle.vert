#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view_projection;
} frame;

layout(push_constant) uniform PushConstants {
    mat4 model;
};

layout(location = 0) out vec3 fragColor;

const vec3 LIGHT_DIR = normalize(vec3(1.0, 2.0, 3.0));
const vec3 SKY_COLOR = vec3(0.53, 0.71, 0.92);
const float FOG_START = 250.0;
const float FOG_END = 500.0;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    gl_Position = frame.view_projection * worldPos;

    // Half-Lambert lighting
    vec3 N = normalize(mat3(model) * inNormal);
    float NdotL = dot(N, LIGHT_DIR);
    float halfLambert = NdotL * 0.5 + 0.5;
    halfLambert = halfLambert * halfLambert;
    vec3 lit = halfLambert * inColor;

    // Distance fog
    float dist = gl_Position.w;
    float fog = clamp((dist - FOG_START) / (FOG_END - FOG_START), 0.0, 1.0);

    fragColor = mix(lit, SKY_COLOR, fog);
}
