#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "definitions.glsl"
#include "shading.glsl"
#include "visibilityBufferUtils.glsl"
#include "geometryPassOpaqueParameters.glsl"

layout(location = 0) in flat uint inDrawID;

layout(location = 0) out uint outVisibilityData;

void main() {
    outVisibilityData = encodeVBData(inDrawID, gl_PrimitiveID);
}
