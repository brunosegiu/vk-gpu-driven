#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "transparentPassParameters.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uvec3 inPackedNormalTexCoordTangent;

layout(location = 0) out vec3 outWorldSpacePos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out flat Material outMaterial; // uses 3..10
layout(location = 11) out vec4 outShadowCoord;
layout(location = 12) out mat3 outTBN;

void main() {
    uint globalDrawIndex = uDrawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    gl_Position = uCameraParameters.viewProjection * meshData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedNormalTexCoordTangent.y);

    vec3 unpackedNormal = unpackSnorm4x8(inPackedNormalTexCoordTangent.x).xyz;
    outNormal = normalize(meshData.normalTransform * unpackedNormal);

    outWorldSpacePos = (meshData.modelMatrix * vec4(inPosition, 1.0)).xyz;

    outShadowCoord =  (uLightParameters.viewProjection * meshData.modelMatrix) * vec4(inPosition, 1.0f);

    outMaterial = uMaterials[meshData.materialId];

    vec4 unpackedTangent = unpackSnorm4x8(inPackedNormalTexCoordTangent.z);
    vec3 tangent = normalize(meshData.normalTransform * unpackedTangent.xyz);
    vec3 bitangent = normalize(cross(outNormal, tangent) * unpackedTangent.w);

    outTBN = mat3(tangent, bitangent, outNormal);
}
