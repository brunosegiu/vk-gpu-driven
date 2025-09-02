#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "glossyReflectionsParameters.glsl"

layout(location = ColorPayloadIndex) rayPayloadInEXT RayPayload rayPayload;

void main() {
    ProceduralSkyShaderParameters params = initSkyShaderParameters(-uLightParameters.direction);
    params.lightColor = normalize(uLightParameters.radiance);

    rayPayload.color += getProceduralSkyColor(params, gl_WorldRayDirectionEXT, 0);
    rayPayload.depth += 1;
    rayPayload.hitDepth = gl_RayTmaxEXT;
}