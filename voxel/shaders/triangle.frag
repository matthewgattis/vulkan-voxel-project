#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormalWorld;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    vec3 N = normalize(fragNormalWorld);

    float NdotL = dot(N, lightDir);
    float halfLambert = NdotL * 0.5 + 0.5;
    halfLambert = halfLambert * halfLambert;

    vec3 lit = halfLambert * fragColor;
    outColor = vec4(lit, 1.0);
}
