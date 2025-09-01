#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "transparentPassParameters.glsl"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in flat Material inMaterial; // uses 3..10
layout(location = 11) in vec4 inShadowCoord;
layout(location = 12) in mat3 inTBN;

layout(location = 0) out vec4 outColor;

vec4 sampleTexture(int index, vec2 uv) {
    return texture(
            sampler2D(uSceneTextures[nonuniformEXT(index)], uTextureSampler),
            uv);
}

void main() {
    const Material material = inMaterial;

    vec4 albedo = vec4(material.albedo.rgb, 1.0f);
    if (material.albedoTextureIndex >= 0) {
        albedo *= sampleTexture(material.albedoTextureIndex, inTexCoord);
    }

    vec3 emissive = material.emissive.rgb;
    if (material.emissiveTextureIndex >= 0) {
        emissive = sampleTexture(material.emissiveTextureIndex, inTexCoord).rgb;
    }

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTextureIndex >= 0) {
        vec2 metallicRoughness = sampleTexture(material.metallicRoughnessTextureIndex, inTexCoord).rg;
        roughness = metallicRoughness.r;
        metallic = metallicRoughness.g;
    }

    vec3 normal = normalize(inNormal);
    if (material.normalTextureIndex >= 0) {
        vec3 normalSample = sampleTexture(material.normalTextureIndex, inTexCoord).xyz;
        vec3 normalTangentSpace = normalize(normalSample * 2.0 - 1.0);
        normal = normalize(inTBN * normalTangentSpace);
    }

    vec4 shadowCoord = (ShadowBiasMat * uLightParameters.viewProjection) * vec4(inWorldSpacePos, 1.0f);
    float shadowDepth = encodeViewDepth(
        inWorldSpacePos.xyz,
        uLightParameters.view,
        uLightParameters.shadowNear,
        uLightParameters.shadowFar);
    float shadowTerm = filterVSM(shadowCoord.xy / shadowCoord.w, shadowDepth, uTextureSampler, uShadowMap);

    vec3 viewVector = normalize(uCameraParameters.cameraPos.xyz - inWorldSpacePos);

    vec3 color = evalLighting(normal, viewVector, -uLightParameters.direction, uLightParameters.radiance, shadowTerm, albedo, metallic, roughness, 1.0, vec3(0.0f), emissive);

    outColor = vec4(gammaCorrection(color), albedo.a);
}
