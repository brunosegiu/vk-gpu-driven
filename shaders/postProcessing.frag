#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "postProcessingParameters.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

// From: https://www.shadertoy.com/view/dtyfRw
vec3 SimpleAces(in vec3 c) {
    float a = 2.51f;
    float b = 0.03f;
    float y = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return clamp((c * (a * c + b)) / (c * (y * c + d) + e), 0.0, 1.0);
}

void main() {
    ivec2 screenSize = textureSize(sampler2D(uScreenTexture, uFrameBufferTextureSampler), 0);
    vec4 color = texelFetch(
            sampler2D(uScreenTexture, uFrameBufferTextureSampler),
            ivec2(inTexCoord * screenSize),
            0);
    vec3 tonemappedColor = SimpleAces(color.rgb);
    vec3 gammaCorrectedColor = gammaCorrection(tonemappedColor);
    outColor = vec4(gammaCorrectedColor, color.a);
}
