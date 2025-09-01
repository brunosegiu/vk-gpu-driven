#ifndef SHADOW_MOMENTS_BLUR_PARAMETERS_GLSL
#define SHADOW_MOMENTS_BLUR_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    ShadowControlData uShadowBlurData;
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler uLinearSampler;
layout(binding = 1, set = UPDATE_ONCE) uniform texture2D uDepthBuffer;

#endif