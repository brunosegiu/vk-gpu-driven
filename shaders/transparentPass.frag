#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in flat uint inDrawID;
layout(location = 4) in vec4 inShadowCoord;
layout(location = 5) in mat3 inTBN;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    mat4 viewProjection;
    vec4 cameraPos;
} CameraParameters;

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    vec3 radiance;
    vec3 direction;
    mat4 viewProjection;
} LightParameters;

layout(binding = 2, set = UPDATE_PER_FRAME, scalar) readonly buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

layout(binding = 3, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint drawData[];
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler textureSampler;
layout(binding = 1, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material Materials[];
};
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D shadowMap;
layout(binding = 3, set = UPDATE_ONCE) uniform texture2D sceneTextures[];

vec4 sampleTexture(int index, vec2 uv) {
    return texture(
            sampler2D(sceneTextures[nonuniformEXT(index)], textureSampler),
            uv);
}

void main() {
    DrawData drawData = SceneData.perDrawData[inDrawID];

    uint materialId = drawData.materialId;
    Material material = Materials[materialId];

    vec4 albedo = vec4(material.albedo.rgb, 0.5f);
    if (material.albedoTextureIndex >= 0) {
        albedo = sampleTexture(material.albedoTextureIndex, inTexCoord);
    }

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTextureIndex >= 0) {
        vec2 metallicRoughness = sampleTexture(material.metallicRoughnessTextureIndex, inTexCoord).rg;
        roughness = metallicRoughness.r;
        metallic = metallicRoughness.g;
    }

    vec3 normal = normalize(inNormal);
    if (material.normalTextureIndex > 0) {
        vec3 normalSample = sampleTexture(material.normalTextureIndex, inTexCoord).xyz;
        vec3 normalTangentSpace = normalize(normalSample * 2.0 - 1.0);
        normal = normalize(inTBN * normalTangentSpace);
    }

	vec4 shadowCoord = inShadowCoord / inShadowCoord.w;
    float shadowTerm = 0.0f;
    {
		shadowTerm = filterPCF(shadowCoord, textureSampler, shadowMap);
	    shadowTerm = clamp(shadowTerm, 0.0f, 1.0f);
    }

    vec3 viewVector = normalize(CameraParameters.cameraPos.xyz - inWorldSpacePos);

    vec3 color = evalLighting(normal, viewVector, -LightParameters.direction, LightParameters.radiance, shadowTerm, albedo, metallic, roughness);

    outColor = vec4(gammaCorrection(color), albedo.a);
}
