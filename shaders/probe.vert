#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "probeParameters.glsl"
#include "ddgiUtils.glsl"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out flat uint outProbeIndex;

void main() {
    outNormal = inPosition;
    outProbeIndex = gl_InstanceIndex;

	const uvec3 gridIndex = probeIndexToGridIndex(outProbeIndex, uDDGI);
	const vec3 worldPos = gridIndexToWorldPos(ivec3(gridIndex), uDDGI);
    gl_Position = uCameraParameters.viewProjection * vec4(inPosition * uDDGI.probeRadius + worldPos, 1.0);
}
