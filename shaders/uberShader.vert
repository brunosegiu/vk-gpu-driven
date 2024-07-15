#version 450

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1

layout(binding = 0, set = UPDATE_PER_FRAME) uniform TCameraParameters {
    mat4 view;
    mat4 projection;
} CameraParameters;

layout( push_constant ) uniform TPushConstants {
	mat4 modelMatrix;
} PerDrawParameters;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outWorldSpacePos;

void main() {
    gl_Position = CameraParameters.projection * CameraParameters.view * PerDrawParameters.modelMatrix * vec4(inPosition, 1.0);
    outWorldSpacePos = (PerDrawParameters.modelMatrix * vec4(inPosition, 1.0)).xyz;
}
