#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "depthOnlyShaderAlphaMaskedParameters.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inDrawID;

layout(location = 0) out float outMoment;

void main() {
    const DrawData drawData = uPersistentSceneData[inDrawID];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    const uint materialId = meshData.materialId;
    const Material material = uMaterials[materialId];
    vec3 albedo = material.albedo.rgb;
    if (material.albedoTextureIndex >= 0) {
        vec4 albedoAlpha =
            texture(sampler2D(uSceneTextures[material.albedoTextureIndex], uTextureSampler), inTexCoord)
                .rgba;
        albedo =  albedoAlpha.rgb;
        if (albedoAlpha.a - 0.5f < 0.0f) {
            discard;
        }
    }

    outMoment = exp(uLightParameters.esmControl * gl_FragCoord.z);
}
