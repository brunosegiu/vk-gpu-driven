#ifndef UPDATE_PROBES_PARAMETERS_GLSL
#define UPDATE_PROBES_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TDDGIData {
    DDGIData uDDGI;
};
layout(binding = 1, set = UPDATE_PER_FRAME, r11f_g11f_b10f) readonly uniform image2DArray uPrevIrradianceTargets;
layout(binding = 2, set = UPDATE_PER_FRAME, rg16f) readonly uniform image2DArray uPrevProbeMoments;
layout(binding = 3, set = UPDATE_PER_FRAME, r11f_g11f_b10f) writeonly uniform image2DArray uConvolutedProbeIrradianceTargets;
layout(binding = 4, set = UPDATE_PER_FRAME, rg16f) writeonly uniform image2DArray uProbeMoments;

layout(binding = 0, set = UPDATE_ONCE) uniform sampler uFramebufferSampler;
layout(binding = 1, set = UPDATE_ONCE) uniform texture2D uProbeRadianceTargets;
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D uProbeDirectionDepth;

#endif