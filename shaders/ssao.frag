#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "visibilityBufferUtils.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out float outVisibilityFactor;

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData CameraParameters;
};

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) uniform TSSAOControlData {
    SSAOControlData ControlData;
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler frameBufferTextureSampler;
layout(binding = 1, set = UPDATE_ONCE) uniform texture2D depthBuffer;

// LUT of circle roatation directions
const uint ROT_COUNT = 16;
const vec2 HEMISPHERE_ROTATION_LUT[ROT_COUNT] = vec2[](
    vec2( 1.0000000,  0.0000000), vec2( 0.9238795,  0.3826834),
    vec2( 0.7071068,  0.7071068), vec2( 0.3826834,  0.9238795),
    vec2( 0.0000000,  1.0000000), vec2(-0.3826834,  0.9238795),
    vec2(-0.7071068,  0.7071068), vec2(-0.9238795,  0.3826834),
    vec2(-1.0000000,  0.0000000), vec2(-0.9238795, -0.3826834),
    vec2(-0.7071068, -0.7071068), vec2(-0.3826834, -0.9238795),
    vec2( 0.0000000, -1.0000000), vec2( 0.3826834, -0.9238795),
    vec2( 0.7071068, -0.7071068), vec2( 0.9238795, -0.3826834)
);

vec2 getRandomHemisphereRotation() {
    uint hash = uint(gl_FragCoord.x) * 73856093u ^ uint(gl_FragCoord.y) * 19349663u;
    uint index = hash % ROT_COUNT;
    return HEMISPHERE_ROTATION_LUT[index];
}

vec3 reconstructViewSpacePos(vec2 inTexCoord, float deviceDepth) {
    vec4 ndc = vec4(inTexCoord * 2.0 - 1.0, deviceDepth * 2.0 - 1.0, 1.0);
    vec4 viewSpacePosPersp = CameraParameters.invProjection * ndc;
    return viewSpacePosPersp.xyz / viewSpacePosPersp.w;
}

vec3 reconstructNormal(vec2 inTexCoord, float deviceDepth, vec2 texelSize) {
    vec2 uv0 = inTexCoord;
    vec2 uv1 = inTexCoord + vec2(1, 0) * texelSize;
    vec2 uv2 = inTexCoord + vec2(0, 1) * texelSize;

    float deviceDepth0 = texture(sampler2D(depthBuffer, frameBufferTextureSampler), uv0).r;
    float deviceDepth1 = texture(sampler2D(depthBuffer, frameBufferTextureSampler), uv1).r;
    float deviceDepth2 = texture(sampler2D(depthBuffer, frameBufferTextureSampler), uv2).r;

    vec3 p0 = reconstructViewSpacePos(uv0, deviceDepth0);
    vec3 p1 = reconstructViewSpacePos(uv1, deviceDepth1);
    vec3 p2 = reconstructViewSpacePos(uv2, deviceDepth2);

    return normalize(cross(p2 - p0, p1 - p0));
}

// https://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint idx, uint num) {
    return vec2( (float(idx) + 0.5) / float(num), radicalInverseVdC(idx) );
}

// Hemisphere sampling from: https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/globals.hlsli#L1333
vec3 hemisphereSampleCosine(vec2 uv) {
	float phi = uv.y * 2 * PI;
	float cosTheta = sqrt(1 - uv.x);
	float sinTheta = sqrt(1 - cosTheta * cosTheta);
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Base inspiration from: https://learnopengl.com/Advanced-Lighting/SSAO
void main() {

    float deviceDepth = texture(sampler2D(depthBuffer, frameBufferTextureSampler), inTexCoord).r;
    if (deviceDepth >= 1.0) {
        outVisibilityFactor = 0.0;
        return;
    }

    vec3 viewSpacePos = reconstructViewSpacePos(inTexCoord, deviceDepth);

    vec2 fbSize = vec2(textureSize(sampler2D(depthBuffer, frameBufferTextureSampler), 0));
    vec2 texelSize = 1.0 / fbSize;

    vec3 randomRotation = vec3(getRandomHemisphereRotation(), 0.0);
    vec3 viewSpaceNormal = reconstructNormal(inTexCoord, deviceDepth, texelSize);
    vec3 viewSpaceTangent = normalize(randomRotation - viewSpaceNormal * dot(randomRotation, viewSpaceNormal));
    vec3 viewSpaceBitangent = cross(viewSpaceNormal, viewSpaceTangent);

    mat3 TBN = mat3(viewSpaceTangent, viewSpaceBitangent, viewSpaceNormal);

    const float radius = ControlData.radius;
    const float power = ControlData.power;
    const uint kernelSize = ControlData.kernelSize;

    float occlusion = 0.0;
    for (uint sampleIndex = 0; sampleIndex < kernelSize; ++sampleIndex) {
        // Sample more directions toward origin: https://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
        vec2 xi = hammersley(sampleIndex, kernelSize);
        float indexDelta = float(sampleIndex) / float(kernelSize);
        float dirBias = mix(0.12, 1.0, indexDelta * indexDelta);
        vec3 hemisphereDir = hemisphereSampleCosine(xi) * dirBias;

        vec3 samplePos = viewSpacePos + TBN * hemisphereDir * radius;
        vec4 clipSpacePos = CameraParameters.projection * vec4(samplePos, 1.0);
        vec2 projectedSampleUV = (clipSpacePos.xy / clipSpacePos.w) * 0.5 + 0.5;
        if (any(lessThan(projectedSampleUV, vec2(0.0))) || any(greaterThan(projectedSampleUV, vec2(1.0)))) continue;
        float sampleDeviceDepth = texture(sampler2D(depthBuffer, frameBufferTextureSampler), projectedSampleUV).r;
        if (sampleDeviceDepth >= 1.0) continue;

        vec3 viewSpaceSamplePos = reconstructViewSpacePos(projectedSampleUV, sampleDeviceDepth);
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(samplePos.z - viewSpaceSamplePos.z));
        occlusion += (viewSpaceSamplePos.z > samplePos.z) ? rangeCheck : 0.0;
    }
    occlusion /= kernelSize;
    outVisibilityFactor = pow(1.0 - occlusion, power);
}
