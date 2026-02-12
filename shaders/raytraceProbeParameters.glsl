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
layout(binding = 3, set = UPDATE_PER_FRAME, scalar) uniform TDDGIData {
    DDGIData uDDGI;
};
layout(binding = 4, set = UPDATE_PER_FRAME) uniform texture2DArray uPrevProbeIrradianceTargets;
layout(binding = 5, set = UPDATE_PER_FRAME) uniform texture2DArray uPrevProbeMomentTargets;

layout(binding = 0, set = UPDATE_ONCE, scalar) readonly buffer TSceneData {
    DrawData uPersistentSceneData[];
};
layout(binding = 1, set = UPDATE_ONCE) uniform accelerationStructureEXT uTopLevelAS;
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D uShadowMap;
layout(binding = 3, set = UPDATE_ONCE, r11f_g11f_b10f) writeonly uniform image2D uProbeIrradianceTargets;
layout(binding = 4, set = UPDATE_ONCE, rgba16f) uniform writeonly image2D uProbeDirectionDepth;
layout(binding = 5, set = UPDATE_ONCE) uniform sampler uMaterialTextureSampler;
layout(binding = 6, set = UPDATE_ONCE) uniform sampler uFrameBufferTextureSampler;
layout(binding = 7, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material uMaterials[];
};
layout(binding = 8, set = UPDATE_ONCE, scalar) readonly buffer Index {
    uint uIndices[];
};
layout(binding = 9, set = UPDATE_ONCE, scalar) readonly buffer VertexPosition {
    vec3 uPositions[];
};
layout(binding = 10, set = UPDATE_ONCE, scalar) readonly buffer PackedNormalTexCoordTangent {
    uvec3 uPackedNormalTexCoordTangent[];
};
layout(binding = 11, set = UPDATE_ONCE) uniform texture2D uSceneTextures[];

#endif