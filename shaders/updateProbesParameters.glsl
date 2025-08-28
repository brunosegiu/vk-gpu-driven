#ifndef UPDATE_PROBES_PARAMETERS_GLSL
#define UPDATE_PROBES_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TDDGIData {
    DDGIData uDDGI;
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler uFramebufferSampler;
layout(binding = 1, set = UPDATE_ONCE) uniform texture2D uProbeIrradianceTargets;
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D uProbeDirectionDepth;
layout(binding = 3, set = UPDATE_ONCE, r11f_g11f_b10f) writeonly uniform image2DArray uConvolutedProbeIrradianceTargets;
layout(binding = 4, set = UPDATE_ONCE, rg16f) uniform writeonly image2DArray uProbeMoments;

#endif