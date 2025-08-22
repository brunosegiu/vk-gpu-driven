#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "visibilityBufferUtils.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inDrawID;

layout(location = 0) out uint outVisibilityData;

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TMeshData {
    MeshData uMeshData[];
};
layout(binding = 2, set = UPDATE_PER_FRAME) buffer readonly DrawCallIDs {
	uint uDrawData[];
};

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};
layout(binding = 1, set = UPDATE_ONCE) uniform sampler uTextureSampler;
layout(binding = 2, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material uMaterials[];
};
layout(binding = 3, set = UPDATE_ONCE) uniform texture2D uSceneTextures[];

void main() {
    const DrawData drawData = uPersistentSceneData[inDrawID];
    const MeshData meshData = uMeshData[drawData.meshIndex];

    uint materialId = meshData.materialId;
    Material material = uMaterials[materialId];

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

    outVisibilityData = encodeVBData(inDrawID, gl_PrimitiveID);
}