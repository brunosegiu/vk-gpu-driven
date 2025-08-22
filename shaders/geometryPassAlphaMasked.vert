#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TMeshData {
    MeshData uMeshData[];
};
layout(binding = 2, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint uDrawData[];
};

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outDrawID;

void main() {
    uint globalDrawIndex = uDrawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    gl_Position = uCameraParameters.viewProjection * meshData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    outDrawID = globalDrawIndex;
}
