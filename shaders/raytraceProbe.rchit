#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : enable

#include "definitions.glsl"

layout(location = ColorPayloadIndex) rayPayloadInEXT RayPayload rayPayload;
layout(location = ShadowPayloadIndex) rayPayloadEXT float shadowAttenuation;
hitAttributeEXT vec2 hitAttributes;

layout(binding = 0, set = UPDATE_PER_FRAME, scalar) uniform TCameraParameters {
    CameraData CameraParameters;
};

layout(binding = 0, set = UPDATE_ONCE) uniform accelerationStructureEXT topLevelAS;

void main() {
    if (rayPayload.depth > MaxRecursionLevel) {
        rayPayload.color = vec3(0.0f);
        return;
    }

    rayPayload.depth += 1;
    rayPayload.color += vec3(0.1f);
}
