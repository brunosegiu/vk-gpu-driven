#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    mat4 viewProjection;
    vec4 cameraForwardDir;
} CameraParameters;

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    vec3 radiance;
    vec3 direction;
    mat4 viewProjection;
} LightParameters;

layout(binding = 2, set = UPDATE_PER_FRAME, scalar) readonly buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

layout(binding = 3, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint drawData[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;
layout(location = 2) in uint inPackedNormal;

layout(location = 0) out vec3 outWorldSpacePos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out flat uint outDrawID;
layout(location = 4) out vec4 outShadowCoord;

const mat4 ShadowBiasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

void main() {
    uint actualDrawIndex = drawData[gl_DrawID];
    DrawData perDrawData = SceneData.perDrawData[actualDrawIndex];

    gl_Position = CameraParameters.viewProjection * perDrawData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    vec3 unpackedNormal = unpackSnorm4x8(inPackedNormal).xyz;
    outNormal = normalize(perDrawData.normalTransform * unpackedNormal);

    outWorldSpacePos = (perDrawData.modelMatrix * vec4(inPosition, 1.0)).xyz;

    outShadowCoord =  (ShadowBiasMat * LightParameters.viewProjection * perDrawData.modelMatrix) * vec4(inPosition, 1.0f);

    outDrawID = actualDrawIndex;
}
