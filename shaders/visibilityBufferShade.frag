#version 460

#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "utils.glsl"
#include "shading.glsl"
#include "visibilityBufferUtils.glsl"
#include "ddgiUtils.glsl"
#include "visibilityBufferShadeParameters.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

vec3 sampleProbeIrradiance(uint probeIndex, vec3 direction) {
    vec2 uv = octEncode(normalize(direction));
    return texture(sampler2DArray(uProbeIrradianceTargets, uIrradianceSampler), vec3(uv, float(probeIndex))).rgb;
}

vec2 sampleProbeMoments(uint probeIndex, vec3 direction) {
    vec2 uv = octEncode(normalize(direction));
    return texture(sampler2DArray(uProbeDepthTargets, uFrameBufferTextureSampler), vec3(uv, float(probeIndex))).rg;
}

vec3 ddgiIndirectDiffuse(vec3 worldSpacePosition, vec3 normal, vec3 viewDir, const DDGIData ddgi) {
    ivec3 gridIndex = nearestProbeGridIndex(worldSpacePosition, ddgi);
    vec3 gridPos = (worldSpacePosition - ddgi.probeGridOrigin) / ddgi.probeSpacing;
    vec3 interpolators = clamp(gridPos - vec3(gridIndex), 0.0f, 1.0f);

    vec3 accumulatedRadiance = vec3(0.0f);
    float accumulatedWeight = 0.0;

    for (int oz = 0; oz <= 1; ++oz) {
		for (int oy = 0; oy <= 1; ++oy) {
            for (int ox = 0; ox <= 1; ++ox) {
                ivec3 sampleProbeGridIndex = clamp(ivec3(gridIndex) + ivec3(ox, oy, oz), ivec3(0), ivec3(ddgi.probeGridCount) - 1);
                uint sampleProbeIndex = gridIndexToProbeIndex(sampleProbeGridIndex, ddgi);
                vec3 sampleProbePosition = gridIndexToWorldPos(sampleProbeGridIndex, ddgi);

                vec3 dir = worldSpacePosition - sampleProbePosition + 0.15 * normal;
                float r = max(length(dir), 1e-4);
                dir /= r;


                vec2 moments = sampleProbeMoments(sampleProbeIndex, dir);
                float visibility = visibilityFromMoments(r, moments, 0.02);

                float backfaceTest = (dot(-dir, normal) + 1.0f) * 0.5f;

                float tx = (ox == 0) ? (1.0 - interpolators.x) : interpolators.x;
                float ty = (oy == 0) ? (1.0 - interpolators.y) : interpolators.y;
                float tz = (oz == 0) ? (1.0 - interpolators.z) : interpolators.z;
                float trillinearWeight = tx * ty * tz;

                float weight = visibility * backfaceTest * trillinearWeight;


                vec3 irradiance = sampleProbeIrradiance(sampleProbeIndex, normal);
                accumulatedRadiance += irradiance * weight;
                accumulatedWeight += weight;
            }
        }
    }
    vec3 irradiance = (accumulatedWeight > EPSILON) ? (accumulatedRadiance / accumulatedWeight) : vec3(0.0f);
    return irradiance * PI * 0.5f;
}

vec4 sampleTexture(int index, InterpolatedWithDerivsVec2 uv) {
    return textureGrad(
            sampler2D(uSceneTextures[nonuniformEXT(index)], uMaterialTextureSampler),
            uv.value,
            uv.ddx,
            uv.ddy);
}

void main() {
    ivec2 vbSize = textureSize(usampler2D(uVisibilityBuffer, uFrameBufferTextureSampler), 0);
    uint encodedVbData = texelFetch(
            usampler2D(uVisibilityBuffer, uFrameBufferTextureSampler),
            ivec2(inTexCoord * vbSize),
            0).x;

    vec2 ndc = inTexCoord * 2.0 - 1.0;

    if (encodedVbData == PRIMITIVE_ID_NONE) {
        ProceduralSkyShaderParameters params = initSkyShaderParameters(-uLightParameters.direction);
        params.lightColor = normalize(uLightParameters.radiance);
        vec3 viewDir = viewDirFromViewProjection(uCameraParameters.invViewProjection, ndc);
        outColor = vec4(getProceduralSkyColor(params, viewDir, 0), 1.0);
        return;
    }

    uvec2 decodedVertexData = decodeVBData(encodedVbData);

    uint drawId = decodedVertexData.x;
    const DrawData drawData = uPersistentSceneData[drawId];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    uint triangleIndex = decodedVertexData.y * 3 + drawData.firstIndex;

    uint vertexIndexA = uIndices[triangleIndex] + drawData.vertexOffset;
    uint vertexIndexB = uIndices[triangleIndex + 1] + drawData.vertexOffset;
    uint vertexIndexC = uIndices[triangleIndex + 2] + drawData.vertexOffset;

    vec3 posA = uPositions[vertexIndexA];
    vec3 posB = uPositions[vertexIndexB];
    vec3 posC = uPositions[vertexIndexC];

    posA = (meshData.modelMatrix * vec4(posA, 1.0)).xyz;
    posB = (meshData.modelMatrix * vec4(posB, 1.0)).xyz;
    posC = (meshData.modelMatrix * vec4(posC, 1.0)).xyz;

    BarycentricDeriv bary = CalcFullBary(
        uCameraParameters.viewProjection * vec4(posA, 1.0),
        uCameraParameters.viewProjection * vec4(posB, 1.0),
        uCameraParameters.viewProjection * vec4(posC, 1.0),
        ndc,
        vbSize
    );

    vec3 worldPos = interpolate(bary, posA, posB, posC);

    vec2 texCoordA = unpackHalf2x16(uPackedTexCoord[vertexIndexA]);
    vec2 texCoordB = unpackHalf2x16(uPackedTexCoord[vertexIndexB]);
    vec2 texCoordC = unpackHalf2x16(uPackedTexCoord[vertexIndexC]);
    InterpolatedWithDerivsVec2 uvWithDerivs = interpolateWithDerivs(
        bary,
        texCoordA,
        texCoordB,
        texCoordC
    );
    vec2 uv = uvWithDerivs.value;

    vec3 unpackedNormalA = normalize(unpackSnorm4x8(uPackedNormal[vertexIndexA]).xyz);
    vec3 unpackedNormalB = normalize(unpackSnorm4x8(uPackedNormal[vertexIndexB]).xyz);
    vec3 unpackedNormalC = normalize(unpackSnorm4x8(uPackedNormal[vertexIndexC]).xyz);

    vec3 normal = normalize(meshData.normalTransform * interpolate(bary, unpackedNormalA, unpackedNormalB, unpackedNormalC));
    
    vec4 unpackedTangentA = unpackSnorm4x8(uPackedTangent[vertexIndexA]);
    vec4 unpackedTangentB = unpackSnorm4x8(uPackedTangent[vertexIndexB]);
    vec4 unpackedTangentC = unpackSnorm4x8(uPackedTangent[vertexIndexC]);

    vec4 tangent = interpolate(bary, unpackedTangentA, unpackedTangentB, unpackedTangentC);
    vec3 normalizedTangent = normalize(meshData.normalTransform * tangent.xyz);
    vec3 bitangent = normalize(cross(normal, normalizedTangent) * tangent.w);

    mat3 TBN = mat3(normalizedTangent, bitangent, normal);

    uint materialId = meshData.materialId;
    Material material = uMaterials[materialId];

    if (material.normalTextureIndex >= 0) {
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

    vec4 shadowCoord = (ShadowBiasMat * uLightParameters.viewProjection) * vec4(worldPos, 1.0f);
	shadowCoord = shadowCoord / shadowCoord.w;
    float shadowTerm = 0.0f;
    {
		shadowTerm = filterPCF(shadowCoord, uLightParameters.shadowTaps, uIrradianceSampler, uShadowMap);
	    shadowTerm = clamp(shadowTerm, 0.0f, 1.0f);
    }

    vec3 viewVector = normalize(uCameraParameters.cameraPos.xyz - worldPos);

    float visibility = texture(sampler2D(uSSAOBuffer, uFrameBufferTextureSampler), inTexCoord).r;

    vec3 indirect = ddgiIndirectDiffuse(worldPos, normal, viewVector, uDDGI); 

    vec3 color = evalLighting(normal, viewVector, -uLightParameters.direction, uLightParameters.radiance, shadowTerm, albedo, metallic, roughness, visibility, indirect);

    outColor = vec4(gammaCorrection(color), 1.0);
}
