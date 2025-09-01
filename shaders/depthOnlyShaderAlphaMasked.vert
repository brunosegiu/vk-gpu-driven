#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "depthOnlyShaderAlphaMaskedParameters.glsl"
#include "shading.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outDrawID;
layout(location = 2) out float outViewSpaceDepth;

void main() {
    uint globalDrawIndex = uDrawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    vec4 worldSpacePos = meshData.modelMatrix * vec4(inPosition, 1.0);

    gl_Position = uLightParameters.viewProjection * worldSpacePos;

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    outDrawID = globalDrawIndex;

    outViewSpaceDepth = encodeViewDepth(
        worldSpacePos.xyz,
        uLightParameters.view,
        uLightParameters.shadowNear,
        uLightParameters.shadowFar);
}
