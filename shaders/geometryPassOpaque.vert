#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "geometryPassOpaqueParameters.glsl"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out flat uint outDrawID;

void main() {
    uint globalDrawIndex = uDrawData[gl_DrawID];

    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    gl_Position = uCameraParameters.viewProjection * meshData.modelMatrix * vec4(inPosition, 1.0);

    outDrawID = globalDrawIndex;
}
