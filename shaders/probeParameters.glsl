#ifndef PROBE_PARAMETERS_GLSL
#define PROBE_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TDDGIData {
    DDGIData uDDGI;
};
layout(binding = 2, set = UPDATE_PER_FRAME) uniform texture2DArray uProbeIrradianceTargets;

layout(binding = 0, set = UPDATE_ONCE) uniform sampler uFrameBufferTextureSampler;

#endif