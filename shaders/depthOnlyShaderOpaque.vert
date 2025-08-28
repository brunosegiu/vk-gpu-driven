#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "depthOnlyShaderOpaqueParameters.glsl"

layout(location = 0) in vec3 inPosition;

void main() {
    uint globalDrawIndex = drawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    gl_Position = uLightParameters.viewProjection * meshData.modelMatrix * vec4(inPosition, 1.0);
}
