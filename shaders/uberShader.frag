#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(cross(dFdx(inPosition), dFdy(inPosition)));
    float nDotL = clamp(dot(normal, normalize(vec3(0, -0.4, 0.3))), 0.0f, 1.0f);

    outColor = vec4(vec3(0.3) * nDotL, 1.0);
}
