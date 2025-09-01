#version 460

#include "depthOnlyShaderOpaqueParameters.glsl"

layout(location = 0) out float outMoment;

void main() {
    outMoment = exp(uLightParameters.esmControl * gl_FragCoord.z);
}
