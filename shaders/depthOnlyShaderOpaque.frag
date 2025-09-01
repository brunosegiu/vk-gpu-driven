#version 460

#include "depthOnlyShaderOpaqueParameters.glsl"

layout(location = 0) in float inViewSpaceDepth;

layout(location = 0) out vec2 outMoments;

void main() {
    float dx = dFdx(inViewSpaceDepth);
    float dy = dFdy(inViewSpaceDepth);
    float depth2 = inViewSpaceDepth * inViewSpaceDepth + 0.25f * (dx * dx + dy * dy);
    outMoments = vec2(inViewSpaceDepth, depth2);
}
