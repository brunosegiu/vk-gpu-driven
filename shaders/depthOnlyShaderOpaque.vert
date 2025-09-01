#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "depthOnlyShaderOpaqueParameters.glsl"
#include "shading.glsl"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out float outViewSpaceDepth;

void main() {
    uint globalDrawIndex = uDrawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    vec4 worldSpacePos = meshData.modelMatrix * vec4(inPosition, 1.0);

    gl_Position = uLightParameters.viewProjection * worldSpacePos;

    outViewSpaceDepth = encodeViewDepth(
        worldSpacePos.xyz,
        uLightParameters.view,
        uLightParameters.shadowNear,
        uLightParameters.shadowFar);
}
