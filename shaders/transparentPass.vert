#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "shading.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    LightData uLightParameters;
};
layout(binding = 2, set = UPDATE_PER_FRAME, scalar) readonly buffer TMeshData {
    MeshData uMeshData[];
};
layout(binding = 3, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint uDrawData[];
};

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};
layout(binding = 1, set = UPDATE_ONCE) uniform sampler uTextureSampler;
layout(binding = 2, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material uMaterials[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;
layout(location = 2) in uint inPackedNormal;
layout(location = 3) in uint inPackedTangent;

layout(location = 0) out vec3 outWorldSpacePos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out flat Material outMaterial;  // uses 3..8
layout(location = 9) out vec4 outShadowCoord;
layout(location = 10) out mat3 outTBN;

void main() {
    uint globalDrawIndex = uDrawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    gl_Position = uCameraParameters.viewProjection * meshData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    vec3 unpackedNormal = unpackSnorm4x8(inPackedNormal).xyz;
    outNormal = normalize(meshData.normalTransform * unpackedNormal);

    outWorldSpacePos = (meshData.modelMatrix * vec4(inPosition, 1.0)).xyz;

    outShadowCoord =  (ShadowBiasMat * uLightParameters.viewProjection * meshData.modelMatrix) * vec4(inPosition, 1.0f);

    outMaterial = uMaterials[meshData.materialId];

    vec4 unpackedTangent = unpackSnorm4x8(inPackedTangent);
    vec3 tangent = normalize(meshData.normalTransform * unpackedTangent.xyz);
    vec3 bitangent = normalize(cross(outNormal, tangent) * unpackedTangent.w);

    outTBN = mat3(tangent, bitangent, outNormal);
}
