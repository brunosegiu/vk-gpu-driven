#ifndef CULLING_PARAMETERS_GLSL
#define CULLING_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCullingData {
    uint ortho;
    vec3 viewDirectionOrCameraPos;
    vec4 frustumPlanes[6];
    uint globalDrawOffset;
    uint maxDrawCount;
} uCullingData;
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TMeshData {
    MeshData uMeshData[];
};
layout(binding = 2, set = UPDATE_PER_FRAME) buffer writeonly CommandBuffer {
	VkDrawIndexedIndirectCommand uOutCommands[];
};
layout(binding = 3, set = UPDATE_PER_FRAME, std430) buffer DrawCallCount {
	uint uOutIndirectDrawCount;
};
layout(binding = 4, set = UPDATE_PER_FRAME, std430) buffer writeonly OutDrawData {
	uint uOutPerDraw[];
};

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};

layout(local_size_x = 64) in;

#endif