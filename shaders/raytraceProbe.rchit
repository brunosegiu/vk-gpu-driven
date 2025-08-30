#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "raytraceProbeParameters.glsl"
#include "ddgiUtils.glsl"

layout(location = ColorPayloadIndex) rayPayloadInEXT RayPayload rayPayload;
layout(location = ShadowPayloadIndex) rayPayloadEXT float shadowAttenuation;

hitAttributeEXT vec2 hitAttributes;

vec4 sampleTexture(int index, vec2 uv) {
    return texture(
            sampler2D(uSceneTextures[nonuniformEXT(index)], uMaterialTextureSampler),
            uv);
}

vec2 interpolate(vec2 a, vec2 b, vec2 c, vec3 barycentricCoords) {
    return a * barycentricCoords.x + b * barycentricCoords.y +
                          c * barycentricCoords.z;
}

vec3 interpolate(vec3 a, vec3 b, vec3 c, vec3 barycentricCoords) {
    return a * barycentricCoords.x + b * barycentricCoords.y +
                          c * barycentricCoords.z;
}

vec4 interpolate(vec4 a, vec4 b, vec4 c, vec3 barycentricCoords) {
    return a * barycentricCoords.x + b * barycentricCoords.y +
                          c * barycentricCoords.z;
}

float traceShadowRay(const vec3 origin, const vec3 direction, float distance) {
    shadowAttenuation = 0.0f;
    traceRayEXT(
        uTopLevelAS,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT |
            gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF,
        0,
        0,
        1,
        origin,
        TMin,
        direction,
        distance,
        ShadowPayloadIndex);
    return shadowAttenuation;
}

void main() {
    if (rayPayload.depth > MaxRecursionLevel) {
        rayPayload.color = vec3(0.0f);
        return;
    }

    const vec3 barycentricCoords = vec3(1.0f - hitAttributes.x - hitAttributes.y, hitAttributes.x, hitAttributes.y);

    uint meshId = gl_InstanceCustomIndexEXT;
    const MeshData meshData = uMeshData[meshId];

    uint triangleIndex = gl_PrimitiveID * 3 + meshData.indexOffset;

    uint vertexIndexA = uIndices[triangleIndex] + meshData.vertexOffset;
    uint vertexIndexB = uIndices[triangleIndex + 1] + meshData.vertexOffset;
    uint vertexIndexC = uIndices[triangleIndex + 2] + meshData.vertexOffset;

    vec3 posA = uPositions[vertexIndexA];
    vec3 posB = uPositions[vertexIndexB];
    vec3 posC = uPositions[vertexIndexC];

    const vec3 position = interpolate(posA, posB, posC, barycentricCoords);
    const vec3 worldSpacePosition = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));

    vec2 texCoordA = unpackHalf2x16(uPackedTexCoord[vertexIndexA]);
    vec2 texCoordB = unpackHalf2x16(uPackedTexCoord[vertexIndexB]);
    vec2 texCoordC = unpackHalf2x16(uPackedTexCoord[vertexIndexC]);
    vec2 uv = interpolate(
        texCoordA,
        texCoordB,
        texCoordC,
        barycentricCoords
    );

    vec3 unpackedNormalA = normalize(unpackSnorm4x8(uPackedNormal[vertexIndexA]).xyz);
    vec3 unpackedNormalB = normalize(unpackSnorm4x8(uPackedNormal[vertexIndexB]).xyz);
    vec3 unpackedNormalC = normalize(unpackSnorm4x8(uPackedNormal[vertexIndexC]).xyz);

    vec3 normal = normalize(meshData.normalTransform * interpolate(unpackedNormalA, unpackedNormalB, unpackedNormalC, barycentricCoords));
    
    vec4 unpackedTangentA = unpackSnorm4x8(uPackedTangent[vertexIndexA]);
    vec4 unpackedTangentB = unpackSnorm4x8(uPackedTangent[vertexIndexB]);
    vec4 unpackedTangentC = unpackSnorm4x8(uPackedTangent[vertexIndexC]);

    vec4 tangent = interpolate(unpackedTangentA, unpackedTangentB, unpackedTangentC, barycentricCoords);
    vec3 normalizedTangent = normalize(meshData.normalTransform * tangent.xyz);
    vec3 bitangent = normalize(cross(normal, normalizedTangent) * tangent.w);

    mat3 TBN = mat3(normalizedTangent, bitangent, normal);

    const uint materialId = meshData.materialId;
    const Material material = uMaterials[materialId];

    if (material.normalTextureIndex >= 0) {
        vec3 normalSample = sampleTexture(material.normalTextureIndex, uv).xyz;
        vec3 normalTangentSpace = normalize(normalSample * 2.0 - 1.0);
        normal = normalize(TBN * normalTangentSpace);
    }

    vec4 albedo = vec4(material.albedo.rgb, 0.5f);
    if (material.albedoTextureIndex >= 0) {
        albedo = sampleTexture(material.albedoTextureIndex, uv);
    }

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTextureIndex >= 0) {
        vec2 metallicRoughness = sampleTexture(material.metallicRoughnessTextureIndex, uv).rg;
        roughness = metallicRoughness.r;
        metallic = metallicRoughness.g;
    }

    const float shadowTerm = traceShadowRay(worldSpacePosition, -uLightParameters.direction, TMax);

    vec3 viewVector = normalize(gl_WorldRayOriginEXT - worldSpacePosition);

    vec3 indirect = ddgiIndirectDiffuse(worldSpacePosition, normal, viewVector, uDDGI, uFrameBufferTextureSampler, uPrevProbeIrradianceTargets, uFrameBufferTextureSampler, uPrevProbeMomentTargets); 

    vec3 color = evalLighting(normal, viewVector, -uLightParameters.direction, uLightParameters.radiance, shadowTerm, albedo, metallic, roughness, 1.0f, indirect);

    rayPayload.depth += 1;
    rayPayload.color += color;
    rayPayload.hitDepth = gl_RayTminEXT + gl_HitTEXT;
}
