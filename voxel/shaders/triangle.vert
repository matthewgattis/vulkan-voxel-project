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
layout(location = 1) out vec3 fragNormalWorld;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    gl_Position = frame.view_projection * worldPos;
    fragNormalWorld = mat3(model) * inNormal;
    fragColor = inColor;
}
