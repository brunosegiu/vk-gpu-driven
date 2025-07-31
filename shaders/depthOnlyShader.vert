#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    vec3 radiance;
    vec3 direction;
    ShadowParameters shadowParameters;
} LightParameters;

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPackedTexCoord;
layout(location = 2) in uint inPackedNormal;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat uint outDrawID;

void main() {
    DrawData perDrawData = SceneData.perDrawData[gl_DrawID];

    gl_Position = LightParameters.shadowParameters.viewProjection * perDrawData.modelMatrix * vec4(inPosition, 1.0);

    outTexCoord = unpackHalf2x16(inPackedTexCoord);

    outDrawID = gl_DrawID;
}
