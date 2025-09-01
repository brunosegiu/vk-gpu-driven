#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "depthOnlyShaderAlphaMaskedParameters.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inDrawID;
layout(location = 2) in float inViewSpaceDepth;

layout(location = 0) out vec2 outMoments;

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

    float dx = dFdx(inViewSpaceDepth);
    float dy = dFdy(inViewSpaceDepth);
    float depth2 = inViewSpaceDepth * inViewSpaceDepth + 0.25f * (dx * dx + dy * dy);
    outMoments = vec2(inViewSpaceDepth, depth2);
}
