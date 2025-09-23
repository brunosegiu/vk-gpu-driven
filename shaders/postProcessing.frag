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

vec3 SimpleReinhard(vec3 x) {
    return x / (1.0 + x);
}

vec3 Uncharted2(vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    vec3 curr = ((x * (A * x + C * B) + D * E) /
                 (x * (A * x +     B) + D * F)) - E / F;

    float W = 11.2;
    float w = ((W * (A * W + C * B) + D * E) /
               (W * (A * W +     B) + D * F)) - E / F;

    return curr * (1.0 / w);
}

// From: www.shadertoy.com/view/ls3GWS
vec3 DoFxaa(vec2 uv) {
    ivec2 screenSize = textureSize(sampler2D(uSceneTexture, uFrameBufferTextureSampler), 0);
    vec2 rcpFrame = 1.0f / vec2(screenSize);
    ivec2 baseCoord = ivec2(uv * screenSize);

    vec3 rgbNW = texelFetch(
            sampler2D(uSceneTexture, uFrameBufferTextureSampler),
            baseCoord + ivec2(-1, -1),
            0).rgb;
    vec3 rgbNE = texelFetch(
            sampler2D(uSceneTexture, uFrameBufferTextureSampler),
            baseCoord + ivec2(1, -1),
            0).rgb;
    vec3 rgbSW = texelFetch(
            sampler2D(uSceneTexture, uFrameBufferTextureSampler),
            baseCoord + ivec2(-1, 1),
            0).rgb;
    vec3 rgbSE = texelFetch(
            sampler2D(uSceneTexture, uFrameBufferTextureSampler),
            baseCoord + ivec2(1, 1),
            0).rgb;
    vec3 rgbM = texelFetch(
            sampler2D(uSceneTexture, uFrameBufferTextureSampler),
            baseCoord,
            0).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    const float reduceMul = 1.0f / uControlData.fxaaMaxSpan;
    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * reduceMul),
        uControlData.fxaaReduceMin);
    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);
    
    dir = min(vec2( uControlData.fxaaMaxSpan,  uControlData.fxaaMaxSpan),
          max(vec2(-uControlData.fxaaMaxSpan, -uControlData.fxaaMaxSpan),
          dir * rcpDirMin)) * rcpFrame.xy;

    vec3 rgbA = (1.0/2.0) * (
        texture(sampler2D(uSceneTexture, uFrameBufferTextureSampler), uv.xy + dir * (1.0/3.0 - 0.5), 0.0).xyz +
        texture(sampler2D(uSceneTexture, uFrameBufferTextureSampler), uv.xy + dir * (2.0/3.0 - 0.5), 0.0).xyz);
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        texture(sampler2D(uSceneTexture, uFrameBufferTextureSampler), uv.xy + dir * (0.0/3.0 - 0.5), 0.0).xyz +
        texture(sampler2D(uSceneTexture, uFrameBufferTextureSampler), uv.xy + dir * (3.0/3.0 - 0.5), 0.0).xyz);
    
    float lumaB = dot(rgbB, luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax)) return rgbA;
    
    return rgbB; 
}

void main() {
    ivec2 screenSize = textureSize(sampler2D(uSceneTexture, uFrameBufferTextureSampler), 0);
    vec3 color;
    if (uControlData.fxaa != 0) {
        color = DoFxaa(inTexCoord);
    } else {
        color = texelFetch(
                sampler2D(uSceneTexture, uFrameBufferTextureSampler),
                ivec2(inTexCoord * screenSize),
                0).rgb;
    }

    vec3 tonemappedColor;
    switch (uControlData.tonemapper) {
        case ToneMapACES:
            tonemappedColor = SimpleAces(color);
            break;
        case ToneMapReinhard:
            tonemappedColor = SimpleReinhard(color);
            break;
        case ToneMapUncharted2:
            tonemappedColor = Uncharted2(color);
            break;
    }
    
    vec3 gammaCorrectedColor = gammaCorrection(tonemappedColor);
    outColor = vec4(gammaCorrectedColor, 1.0f);
}
