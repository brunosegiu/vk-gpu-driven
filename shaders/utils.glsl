#ifndef UTILS_GLSL
#define UTILS_GLSL

vec3 viewDirFromViewProjection(mat4 viewProj, vec2 ndc) {
    vec4 nearClip = vec4(ndc, 0.0, 1.0);
    vec4 farClip  = vec4(ndc, 1.0, 1.0);
    vec4 nearWS = viewProj * nearClip;
    vec4 farWS  = viewProj * farClip;
    nearWS /= nearWS.w;
    farWS  /= farWS.w;
    return normalize(farWS.xyz - nearWS.xyz);
}

float linearizeDepth(float deviceDepth, float far, float near) {
    return (near * far) / max(1e-6, far - deviceDepth * (far - near));
}

#endif