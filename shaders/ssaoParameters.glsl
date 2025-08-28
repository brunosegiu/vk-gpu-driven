#ifndef SSAO_PARAMETERS_GLSL
#define SSAO_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TSSAOControlData {
    SSAOControlData uControlData;
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler frameBufferTextureSampler;
layout(binding = 1, set = UPDATE_ONCE) uniform texture2D depthBuffer;

#endif