#version 460

#include "definitions.glsl"
#include "shadowMomentsBlurVerticalParameters.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out float outMoments;

float readMoments(vec2 uv) {
    return texture(sampler2D(uDepthBuffer, uLinearSampler), uv).r;
}

// From: https://www.shadertoy.com/view/Xd33Rf
float blurGaussianVertical(vec2 uv, vec2 resolution, float radius) {
    float blr = float(0.0);
    blr += 0.026109 * readMoments(uv + vec2(0.0,-15.5) * radius / resolution);
    blr += 0.034202 * readMoments(uv + vec2(0.0,-13.5) * radius / resolution);
    blr += 0.043219 * readMoments(uv + vec2(0.0,-11.5) * radius / resolution);
    blr += 0.052683 * readMoments(uv + vec2(0.0, -9.5) * radius / resolution);
    blr += 0.061948 * readMoments(uv + vec2(0.0, -7.5) * radius / resolution);
    blr += 0.070266 * readMoments(uv + vec2(0.0, -5.5) * radius / resolution);
    blr += 0.076883 * readMoments(uv + vec2(0.0, -3.5) * radius / resolution);
    blr += 0.081149 * readMoments(uv + vec2(0.0, -1.5) * radius / resolution);
    blr += 0.041312 * readMoments(uv + vec2(0.0,  0.0) * radius / resolution);
    blr += 0.081149 * readMoments(uv + vec2(0.0,  1.5) * radius / resolution);
    blr += 0.076883 * readMoments(uv + vec2(0.0,  3.5) * radius / resolution);
    blr += 0.070266 * readMoments(uv + vec2(0.0,  5.5) * radius / resolution);
    blr += 0.061948 * readMoments(uv + vec2(0.0,  7.5) * radius / resolution);
    blr += 0.052683 * readMoments(uv + vec2(0.0,  9.5) * radius / resolution);
    blr += 0.043219 * readMoments(uv + vec2(0.0, 11.5) * radius / resolution);
    blr += 0.034202 * readMoments(uv + vec2(0.0, 13.5) * radius / resolution);
    blr += 0.026109 * readMoments(uv + vec2(0.0, 15.5) * radius / resolution);
    blr /= 0.93423;
    return blr;
}

void main() {
    vec2 shadowMapSize = vec2(textureSize(sampler2D(uDepthBuffer, uLinearSampler), 0));
	outMoments = blurGaussianVertical(inTexCoord, shadowMapSize, uLightParameters.shadowMapBlurRadius);
}
