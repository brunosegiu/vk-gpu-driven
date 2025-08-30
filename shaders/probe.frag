#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "probeParameters.glsl"
#include "ddgiUtils.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in flat uint inProbeIndex;

layout(location = 0) out vec4 outColor;

vec3 sampleProbeIrradiance(uint probeIndex, vec3 direction) {
    vec2 uv = octEncode(normalize(direction));
    return texture(sampler2DArray(uProbeIrradianceTargets, uFrameBufferTextureSampler), vec3(uv, float(probeIndex))).rgb;
}

void main() {
    vec3 normal = normalize(inNormal);
    vec3 irradiance = sampleProbeIrradiance(inProbeIndex, normal);
    outColor = vec4(irradiance, 1.0f);
}
