#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inDrawID;

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    vec3 radiance;
    vec3 direction;
    mat4 viewProjection;
} LightParameters;

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

layout(binding = 0, set = UPDATE_ONCE) uniform sampler textureSampler;
layout(binding = 1, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material Materials[];
};
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D sceneTextures[];

void main() {
    DrawData perDrawData = SceneData.perDrawData[inDrawID];

    uint materialId = perDrawData.materialId;
    Material material = Materials[materialId];
    vec3 albedo = material.albedo.rgb;
    if (material.albedoTextureIndex >= 0) {
        vec4 albedoAlpha =
            texture(sampler2D(sceneTextures[material.albedoTextureIndex], textureSampler), inTexCoord)
                .rgba;
        albedo =  albedoAlpha.rgb;
        if (albedoAlpha.a - 0.5f < 0.0f) {
            discard;
        }
    }
}
