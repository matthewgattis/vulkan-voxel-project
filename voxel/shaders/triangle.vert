#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 projection;
} frame;

layout(push_constant) uniform PushConstants {
    mat4 model;
};

layout(location = 0) out vec3 fragColor;

const vec3 LIGHT_DIR = normalize(vec3(1.0, 2.0, 3.0));
const vec3 SKY_COLOR = vec3(0.53, 0.71, 0.92);
const float FOG_START = 400.0;
const float FOG_END = 500.0;
const float FOG_DENSITY = 2.0;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    vec4 viewPos = frame.view * worldPos;
    gl_Position = frame.projection * viewPos;

    // Half-Lambert lighting
    vec3 N = normalize(mat3(model) * inNormal);
    float NdotL = dot(N, LIGHT_DIR);
    float halfLambert = NdotL * 0.5 + 0.5;
    halfLambert = halfLambert * halfLambert;
    vec3 lit = halfLambert * inColor;

    // Spherical distance fog
    float dist = length(viewPos.xyz);
    float t = clamp((dist - FOG_START) / (FOG_END - FOG_START), 0.0, 1.0);
    float f = FOG_DENSITY * t;
    float fog = 1.0 - exp(-f * f);

    fragColor = mix(lit, SKY_COLOR, fog);
}
