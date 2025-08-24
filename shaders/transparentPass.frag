#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in flat Material inMaterial; // uses 3..8
layout(location = 9) in vec4 inShadowCoord;
layout(location = 10) in mat3 inTBN;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    LightData uLightParameters;
};
layout(binding = 2, set = UPDATE_PER_FRAME, scalar) readonly buffer TMeshData {
    MeshData uMeshData[];
};
layout(binding = 3, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint uDrawData[];
};

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};
layout(binding = 1, set = UPDATE_ONCE) uniform sampler uTextureSampler;
layout(binding = 2, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material uMaterials[];
};
layout(binding = 3, set = UPDATE_ONCE) uniform texture2D uShadowMap;
layout(binding = 4, set = UPDATE_ONCE) uniform texture2D uSceneTextures[];

vec4 sampleTexture(int index, vec2 uv) {
    return texture(
            sampler2D(uSceneTextures[nonuniformEXT(index)], uTextureSampler),
            uv);
}

void main() {
    const Material material = inMaterial;

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
    if (material.normalTextureIndex >= 0) {
        vec3 normalSample = sampleTexture(material.normalTextureIndex, inTexCoord).xyz;
        vec3 normalTangentSpace = normalize(normalSample * 2.0 - 1.0);
        normal = normalize(inTBN * normalTangentSpace);
    }

	vec4 shadowCoord = inShadowCoord / inShadowCoord.w;
    float shadowTerm = 0.0f;
    {
		shadowTerm = filterPCF(shadowCoord, uLightParameters.shadowTaps, uTextureSampler, uShadowMap);
	    shadowTerm = clamp(shadowTerm, 0.0f, 1.0f);
    }

    vec3 viewVector = normalize(uCameraParameters.cameraPos.xyz - inWorldSpacePos);

    vec3 color = evalLighting(normal, viewVector, -uLightParameters.direction, uLightParameters.radiance, shadowTerm, albedo, metallic, roughness, 1.0, vec3(0.0f));

    outColor = vec4(gammaCorrection(color), albedo.a);
}
