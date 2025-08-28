#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "visibilityBufferUtils.glsl"
#include "edgeAwareBoxBlurParameters.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out float outAO;

// Base inspiration from: https://learnopengl.com/Advanced-Lighting/SSAO
void main() {
    outAO = 0.0;

    vec2 fbSize = vec2(textureSize(sampler2D(uDepthBuffer, uFrameBufferTextureSampler), 0));
    vec2 texelSize = 1.0 / fbSize;

    float centerDepth = texture(sampler2D(uDepthBuffer, uFrameBufferTextureSampler), inTexCoord).r;

    const int radius = uControlData.blurRadius;
    float depthTreshold = 0.01;
    float invSampleWeight = 0.0;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            vec2 sampleOffset = vec2(dx, dy) * texelSize;
            vec2 sampleUV = clamp(inTexCoord + sampleOffset, vec2(0), vec2(1));

            float sampleDepth = texture(sampler2D(uDepthBuffer, uFrameBufferTextureSampler), sampleUV).r;

            bool isNotEdge = abs(sampleDepth - centerDepth) <= depthTreshold;

            if (isNotEdge) {
                outAO += texture(sampler2D(uSSAOBuffer, uFrameBufferTextureSampler), sampleUV).r;
                invSampleWeight += 1.0;
            }
        }
    }

    outAO = invSampleWeight > 0.0001 ? outAO / invSampleWeight : 0.0;
}
