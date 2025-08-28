#ifndef DEPTH_ONLY_ALPHA_MASKED_PARAMETERS_GLSL
#define DEPTH_ONLY_ALPHA_MASKED_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    LightData uLightParameters;
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

#endif