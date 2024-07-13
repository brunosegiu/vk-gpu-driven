#version 450

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1

layout(binding = 0, set = UPDATE_PER_FRAME) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outPosition;

void main() {
    gl_Position = ubo.proj * ubo.view * vec4(inPosition, 1.0);
    outPosition = inPosition;
}
