#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    LightData LightParameters;
};

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

layout(binding = 2, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint drawData[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outDrawID;

void main() {
    uint globalDrawIndex = drawData[gl_DrawID];
    DrawData perDrawData = SceneData.perDrawData[globalDrawIndex];

    gl_Position = LightParameters.viewProjection * perDrawData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    outDrawID = globalDrawIndex;
}
