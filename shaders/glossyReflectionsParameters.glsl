#ifndef RAYTRACE_PROBE_PARAMETERS_GLSL
#define RAYTRACE_PROBE_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TLightParameters {
    LightData uLightParameters;
};
layout(binding = 2, set = UPDATE_PER_FRAME, scalar) readonly buffer TMeshData {
    MeshData uMeshData[];
};

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};
layout(binding = 1, set = UPDATE_ONCE) uniform accelerationStructureEXT uTopLevelAS;
layout(binding = 2, set = UPDATE_ONCE) uniform utexture2D uVisibilityBuffer;
layout(binding = 3, set = UPDATE_ONCE) uniform texture2D uShadowMap;
layout(binding = 4, set = UPDATE_ONCE, r11f_g11f_b10f) writeonly uniform image2D uReflectionTarget;
layout(binding = 5, set = UPDATE_ONCE, r16f) uniform writeonly image2D uReflectionHitDepthTarget;
layout(binding = 6, set = UPDATE_ONCE) uniform sampler uMaterialTextureSampler;
layout(binding = 7, set = UPDATE_ONCE) uniform sampler uFrameBufferTextureSampler;
layout(binding = 8, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material uMaterials[];
};
layout(binding = 9, set = UPDATE_ONCE, scalar) readonly buffer Index {
    uint uIndices[];
};
layout(binding = 10, set = UPDATE_ONCE, scalar) readonly buffer VertexPosition {
    vec3 uPositions[];
};
layout(binding = 11, set = UPDATE_ONCE, scalar) readonly buffer PackedTexCoord {
    uint uPackedTexCoord[];
};
layout(binding = 12, set = UPDATE_ONCE, scalar) readonly buffer PackedNormal {
    uint uPackedNormal[];
};
layout(binding = 13, set = UPDATE_ONCE, scalar) readonly buffer PackedTangent {
    uint uPackedTangent[];
};
layout(binding = 14, set = UPDATE_ONCE) uniform texture2D uSceneTextures[];

#endif