#version 450

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

struct Material {
    vec3 albedo;
    vec3 emissive;
    float roughness;
    float metallic;
    float transmission;
    float indexOfRefraction;
    int albedoTextureIndex;
    int roughnessTextureIndex;
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler textureSampler;
layout(binding = 1, set = UPDATE_ONCE, scalar) buffer TMaterial {
    Material values[];
} Materials;
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D sceneTextures[];

layout( push_constant ) uniform TPushConstants {
	mat4 modelMatrix;
    uint materialId;
} PerDrawParameters;

void main() {
    const vec3 lightDir = normalize(vec3(0, -1, 1));
    float nDotL = clamp(dot(normalize(inNormal), lightDir), 0.05f, 1.0f);

    uint materialId = PerDrawParameters.materialId;
    Material material = Materials.values[materialId];
    vec3 albedo = material.albedo.rgb;
    if (material.albedoTextureIndex >= 0) {
        vec4 albedoAlpha =
            texture(sampler2D(sceneTextures[material.albedoTextureIndex], textureSampler), inTexCoord)
                .rgba;
        albedo =  albedoAlpha.rgb;
        //TODO: Alpha test in separate pass
        if (albedoAlpha.a - 0.5f < 0.0f) {
            discard;
        }
    }

    outColor = vec4(albedo * nDotL, 1.0);
}
