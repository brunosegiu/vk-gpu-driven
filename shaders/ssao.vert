#version 460

#extension GL_EXT_scalar_block_layout : enable

#include "definitions.glsl"
#include "ssaoParameters.glsl"

layout(location = 0) out vec2 outTexCoord;

void main() {
    const vec2 pos[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(-1.0,  3.0),
        vec2( 3.0, -1.0)
    );
    vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
	outTexCoord = p * 0.5 + 0.5;
}