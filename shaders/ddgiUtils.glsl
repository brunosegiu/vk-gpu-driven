#ifndef DDGI_UTILS_UTILS_GLSL
#define DDGI_UTILS_UTILS_GLSL

#include "definitions.glsl"

// Octahedral mapping: https://jcgt.org/published/0003/02/01/
// https://discourse.panda3d.org/t/glsl-octahedral-normal-packing/15233

// For each component of v, returns -1 if the component is < 0, else 1
vec2 sign_not_zero(vec2 v) {
	return fma(step(vec2(0.0), v), vec2(2.0), vec2(-1.0));
}

vec2 octEncode(vec3 normal) {
	normal.xy /= dot(abs(normal), vec3(1.0f));
	vec2 uv = mix(normal.xy, (1.0 - abs(normal.yx)) * sign_not_zero(normal.xy), step(normal.z, 0.0));
	return uv * 0.5 + 0.5;
}

vec3 octDecode(vec2 uv) {
	uv = uv * 2.0 - 1.0;
	vec3 normal = vec3(uv.xy, 1.0 - abs(uv.x) - abs(uv.y));
	normal.xy = mix(normal.xy, (1.0 - abs(normal.yx)) * sign_not_zero(normal.xy), step(normal.z, 0.0));
	return normalize(normal);
}

uvec3 probeIndexToGridIndex(uint probeIndex, const DDGIData ddgi) {
	uvec3 result;
	result.x = probeIndex % ddgi.probeGridCount.x;
	result.y = (probeIndex / ddgi.probeGridCount.x) % ddgi.probeGridCount.y;
	result.z = probeIndex / (ddgi.probeGridCount.x * ddgi.probeGridCount.y);
	return result;
}

uint gridIndexToProbeIndex(uvec3 gridIndex, const DDGIData ddgi) {
	return uint(gridIndex.x +
				gridIndex.y * ddgi.probeGridCount.x +
				gridIndex.z * ddgi.probeGridCount.x * ddgi.probeGridCount.y);
}

vec3 gridIndexToWorldPos(ivec3 gridIndex, const DDGIData ddgi) {
	return ddgi.probeGridOrigin + vec3(gridIndex) * ddgi.probeSpacing;
}

ivec3 nearestProbeGridIndex(vec3 position, const DDGIData ddgi) {
    vec3 gridPos = (position - ddgi.probeGridOrigin) / ddgi.probeSpacing;
    return ivec3(clamp(ivec3(floor(gridPos)), ivec3(0), ddgi.probeGridCount - 1));
}

float square(float a) {
    return a * a;
}

// https://www.youtube.com/watch?v=KufJBCTdn_o&t=5s
float visibilityFromMoments(float distanceToSample, vec2 moments, float softness) {
    float mean = moments.x;
    float mean2 = moments.y;
    float variance  = abs(square(mean) - mean2);
    float chebyshevWeight = variance / (variance + square(max(distanceToSample - mean, 0.0f)));
    chebyshevWeight = max(chebyshevWeight * chebyshevWeight * chebyshevWeight, 0.0f);
	return (distanceToSample <= mean) ? 1.0 : chebyshevWeight;
}

#endif