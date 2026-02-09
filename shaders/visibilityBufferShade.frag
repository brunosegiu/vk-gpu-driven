#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "definitions.glsl"
#include "utils.glsl"
#include "shading.glsl"
#include "visibilityBufferUtils.glsl"
#include "ddgiUtils.glsl"
#include "visibilityBufferShadeParameters.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

vec3 sampleGlossyReflection(vec2 uv, float roughness, float worldSpaceDepth, float far) {
    float maxMip = float(textureQueryLevels(sampler2D(uReflectionsBuffer, uIrradianceSampler)) - 1);
    float lod = sqrt(roughness) * maxMip;
    return textureLod(sampler2D(uReflectionsBuffer, uIrradianceSampler), uv, lod).rgb;
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

    vec3 emissive = material.emissive.rgb;
    if (material.emissiveTextureIndex >= 0) {
        emissive = sampleTexture(material.emissiveTextureIndex, uvWithDerivs).rgb;
    }

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTextureIndex >= 0) {
        vec2 metallicRoughness = sampleTexture(material.metallicRoughnessTextureIndex, uvWithDerivs).rg;
        roughness *= metallicRoughness.r;
        metallic *= metallicRoughness.g;
    }

    vec4 shadowCoord = (ShadowBiasMat * uLightParameters.viewProjection) * vec4(worldPos, 1.0f);
    float shadowTerm = filterESM(shadowCoord / shadowCoord.w, uLightParameters.esmControl, uMaterialTextureSampler, uShadowMap);

    vec3 viewVector = normalize(uCameraParameters.cameraPos.xyz - worldPos);

    float visibility = texture(sampler2D(uSSAOBuffer, uFrameBufferTextureSampler), inTexCoord).r;

    vec3 indirect = ddgiIndirectDiffuse(
        worldPos,
        normal,
        viewVector,
        uDDGI,
        uIrradianceSampler,
        uProbeIrradianceTargets,
        uFrameBufferTextureSampler,
        uProbeMomentTargets
     ); 

   float worldSpaceDepth = length(uCameraParameters.cameraPos.xyz - worldPos);
   vec3 glossyIndirect = sampleGlossyReflection(
        inTexCoord,
        roughness,
        worldSpaceDepth,
        uCameraParameters.far
    );

    ShadingParams params;
    params.N = normal;
    params.V = viewVector;
    params.L = -uLightParameters.direction;
    params.radiance = uLightParameters.radiance;
    params.shadowTerm = shadowTerm;
    params.albedo = albedo;
    params.metallic = metallic;
    params.roughness = roughness;
    params.emissive = emissive;
    params.visibility = visibility;
    params.indirectDiffuse = indirect;
    params.indirectGlossy = glossyIndirect;
    params.directWeight = uLightParameters.directWeight;
    params.indirectDiffuseWeight = uLightParameters.indirectDiffuseWeight;
    params.indirectGlossyWeight = uLightParameters.indirectGlossyWeight;

    vec3 color = evalLighting(params);

    outColor = vec4(color, 1.0);
}
