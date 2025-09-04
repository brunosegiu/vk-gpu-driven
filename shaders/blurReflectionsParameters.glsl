#ifndef UPDATE_PROBES_PARAMETERS_GLSL
#define UPDATE_PROBES_PARAMETERS_GLSL

#include "definitions.glsl"

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData uCameraParameters;
};
layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TBlurControlData {
    BlurControlData uControl;
};

layout(binding = 0, set = UPDATE_ONCE) uniform texture2D uInReflections;
layout(binding = 1, set = UPDATE_ONCE) uniform texture2D uInReflectionsDepth;
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D uInDepthBuffer;
layout(binding = 3, set = UPDATE_ONCE) uniform sampler uFramebufferSampler;
layout(binding = 4, set = UPDATE_ONCE, r11f_g11f_b10f) writeonly uniform image2D uOutBlurredReflections;

#endif