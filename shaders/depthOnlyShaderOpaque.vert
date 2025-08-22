#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    LightData uLightParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TMeshData {
    MeshData uMeshData[];
};
layout(binding = 2, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint drawData[];
};

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};

layout(location = 0) in vec3 inPosition;

void main() {
    uint globalDrawIndex = drawData[gl_DrawID];
    const DrawData drawData = uPersistentSceneData[globalDrawIndex];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    gl_Position = uLightParameters.viewProjection * meshData.modelMatrix * vec4(inPosition, 1.0);
}
