#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "depthOnlyShaderAlphaMaskedParameters.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outDrawID;

void main() {
    uint globalDrawIndex = uDrawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    gl_Position = uLightParameters.viewProjection * meshData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    outDrawID = globalDrawIndex;
}
