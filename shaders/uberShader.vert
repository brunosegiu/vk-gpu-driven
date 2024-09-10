#version 450

#extension GL_EXT_scalar_block_layout : enable

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1

layout(binding = 0, set = UPDATE_PER_FRAME) uniform TCameraParameters {
    mat4 viewProjection;
    vec4 cameraForwardDir;
} CameraParameters;

struct DrawData {
	mat4 modelMatrix;
    uint materialId;
    mat3 normalTransform;
};

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

layout( push_constant ) uniform TPushConstants {
    uint drawId;
} PerDrawParameters;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;
layout(location = 2) in uint inPackedNormal;

layout(location = 0) out vec3 outWorldSpacePos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outNormal;

void main() {
    DrawData perDrawData = SceneData.perDrawData[PerDrawParameters.drawId];

    gl_Position = CameraParameters.viewProjection * perDrawData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    vec3 unpackedNormal = unpackSnorm4x8(inPackedNormal).xyz;
    outNormal = normalize(perDrawData.normalTransform * unpackedNormal);

    outWorldSpacePos = (perDrawData.modelMatrix * vec4(inPosition, 1.0)).xyz;
}
