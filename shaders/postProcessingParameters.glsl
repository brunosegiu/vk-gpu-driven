#ifndef SSAO_PARAMETERS_GLSL
#define SSAO_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TPostProcessingControlData {
    PostProcessingControlData uControlData;
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler uFrameBufferTextureSampler;
layout(binding = 1, set = UPDATE_ONCE) uniform texture2D uSceneTexture;

#endif