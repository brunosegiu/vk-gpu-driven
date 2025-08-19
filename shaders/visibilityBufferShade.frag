#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "visibilityBufferUtils.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData CameraParameters;
};

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    LightData LightParameters;
};

layout(binding = 2, set = UPDATE_PER_FRAME, scalar) readonly buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

layout(binding = 0, set = UPDATE_ONCE) uniform sampler materialTextureSampler;
layout(binding = 1, set = UPDATE_ONCE) uniform sampler frameBufferTextureSampler;
layout(binding = 2, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material Materials[];
};
layout(binding = 3, set = UPDATE_ONCE) uniform texture2D shadowMap;
layout(binding = 4, set = UPDATE_ONCE) uniform utexture2D visibilityBuffer;
layout(binding = 5, set = UPDATE_ONCE) uniform texture2D ssaoBuffer;
layout(binding = 6, set = UPDATE_ONCE, scalar) readonly buffer Index {
    uint indices[];
};
layout(binding = 7, set = UPDATE_ONCE, scalar) readonly buffer VertexPosition {
    vec3 positions[];
};
layout(binding = 8, set = UPDATE_ONCE, scalar) readonly buffer PackedTexCoord {
    uint packedTexCoord[];
};
layout(binding = 9, set = UPDATE_ONCE, scalar) readonly buffer PackedNormal {
    uint packedNormal[];
};
layout(binding = 10, set = UPDATE_ONCE, scalar) readonly buffer PackedTangent {
    uint packedTangent[];
};
layout(binding = 11, set = UPDATE_ONCE) uniform texture2D sceneTextures[];

vec4 sampleTexture(int index, InterpolatedWithDerivsVec2 uv) {
    return textureGrad(
            sampler2D(sceneTextures[nonuniformEXT(index)], materialTextureSampler),
            uv.value,
            uv.ddx,
            uv.ddy);
}

vec3 viewDirFromViewProjection(mat4 invViewProj, vec2 ndc) {
    vec4 nearClip = vec4(ndc, 0.0, 1.0);
    vec4 farClip  = vec4(ndc, 1.0, 1.0);
    vec4 nearWS = invViewProj * nearClip;
    vec4 farWS  = invViewProj * farClip;
    nearWS /= nearWS.w;
    farWS  /= farWS.w;
    return normalize(farWS.xyz - nearWS.xyz);
}

void main() {
    ivec2 vbSize = textureSize(usampler2D(visibilityBuffer, frameBufferTextureSampler), 0);
    uint encodedVbData = texelFetch(
            usampler2D(visibilityBuffer, frameBufferTextureSampler),
            ivec2(inTexCoord * vbSize),
            0).x;

    vec2 ndc = inTexCoord * 2.0 - 1.0;

    if (encodedVbData == PRIMITIVE_ID_NONE) {
        ProceduralSkyShaderParameters params = initSkyShaderParameters(-LightParameters.direction);
        params.lightColor = normalize(LightParameters.radiance);
        vec3 viewDir = viewDirFromViewProjection(CameraParameters.invViewProjection, ndc);
        outColor = vec4(getProceduralSkyColor(params, viewDir, 0), 1.0);
        return;
    }

    uvec2 decodedVertexData = decodeVBData(encodedVbData);

    uint drawId = decodedVertexData.x;
    DrawData drawData = SceneData.perDrawData[drawId];

    uint triangleIndex = decodedVertexData.y * 3 + drawData.firstIndex;

    uint vertexIndexA = indices[triangleIndex] + drawData.vertexOffset;
    uint vertexIndexB = indices[triangleIndex + 1] + drawData.vertexOffset;
    uint vertexIndexC = indices[triangleIndex + 2] + drawData.vertexOffset;

    vec3 posA = positions[vertexIndexA];
    vec3 posB = positions[vertexIndexB];
    vec3 posC = positions[vertexIndexC];

    posA = (drawData.modelMatrix * vec4(posA, 1.0)).xyz;
    posB = (drawData.modelMatrix * vec4(posB, 1.0)).xyz;
    posC = (drawData.modelMatrix * vec4(posC, 1.0)).xyz;

    BarycentricDeriv bary = CalcFullBary(
        CameraParameters.viewProjection * vec4(posA, 1.0),
        CameraParameters.viewProjection * vec4(posB, 1.0),
        CameraParameters.viewProjection * vec4(posC, 1.0),
        ndc,
        vbSize
    );

    vec3 worldPos = interpolate(bary, posA, posB, posC);

    vec2 texCoordA = unpackHalf2x16(packedTexCoord[vertexIndexA]);
    vec2 texCoordB = unpackHalf2x16(packedTexCoord[vertexIndexB]);
    vec2 texCoordC = unpackHalf2x16(packedTexCoord[vertexIndexC]);
    InterpolatedWithDerivsVec2 uvWithDerivs = interpolateWithDerivs(
        bary,
        texCoordA,
        texCoordB,
        texCoordC
    );
    vec2 uv = uvWithDerivs.value;

    vec3 unpackedNormalA = normalize(unpackSnorm4x8(packedNormal[vertexIndexA]).xyz);
    vec3 unpackedNormalB = normalize(unpackSnorm4x8(packedNormal[vertexIndexB]).xyz);
    vec3 unpackedNormalC = normalize(unpackSnorm4x8(packedNormal[vertexIndexC]).xyz);

    vec3 normal = normalize(drawData.normalTransform * interpolate(bary, unpackedNormalA, unpackedNormalB, unpackedNormalC));
    
    vec4 unpackedTangentA = normalize(unpackSnorm4x8(packedTangent[vertexIndexA]));
    vec4 unpackedTangentB = normalize(unpackSnorm4x8(packedTangent[vertexIndexB]));
    vec4 unpackedTangentC = normalize(unpackSnorm4x8(packedTangent[vertexIndexC]));

    vec4 tangent = interpolate(bary, unpackedTangentA, unpackedTangentB, unpackedTangentC);
    vec3 normalizedTangent = normalize(drawData.normalTransform * tangent.xyz);
    vec3 bitangent = normalize(cross(normal, normalizedTangent) * tangent.w);

    mat3 TBN = mat3(normalizedTangent, bitangent, normal);

    uint materialId = drawData.materialId;
    Material material = Materials[materialId];

    if (material.normalTextureIndex > 0) {
        vec3 normalSample = sampleTexture(material.normalTextureIndex, uvWithDerivs).xyz;
        vec3 normalTangentSpace = normalize(normalSample * 2.0 - 1.0);
        normal = normalize(TBN * normalTangentSpace);
    }

    vec4 albedo = vec4(material.albedo.rgb, 0.5f);
    if (material.albedoTextureIndex >= 0) {
        albedo = sampleTexture(material.albedoTextureIndex, uvWithDerivs);
    }

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTextureIndex >= 0) {
        vec2 metallicRoughness = sampleTexture(material.metallicRoughnessTextureIndex, uvWithDerivs).rg;
        roughness = metallicRoughness.r;
        metallic = metallicRoughness.g;
    }

    vec4 shadowCoord = (ShadowBiasMat * LightParameters.viewProjection) * vec4(worldPos, 1.0f);
	shadowCoord = shadowCoord / shadowCoord.w;
    float shadowTerm = 0.0f;
    {
		shadowTerm = filterPCF(shadowCoord, LightParameters.shadowTaps, materialTextureSampler, shadowMap);
	    shadowTerm = clamp(shadowTerm, 0.0f, 1.0f);
    }

    vec3 viewVector = normalize(CameraParameters.cameraPos.xyz - worldPos);

    float visibility = texture(sampler2D(ssaoBuffer, frameBufferTextureSampler), inTexCoord).r;

    vec3 color = evalLighting(normal, viewVector, -LightParameters.direction, LightParameters.radiance, shadowTerm, albedo, metallic, roughness, visibility);

    outColor = vec4(gammaCorrection(color), 1.0);
}
