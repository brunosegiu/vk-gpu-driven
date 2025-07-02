#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1

layout(location = 0) in vec3 inWorldSpacePos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in flat uint inDrawID;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = UPDATE_PER_FRAME) uniform TCameraParameters {
    mat4 viewProjection;
    vec4 cameraForwardDir;
} CameraParameters;

struct DrawData {
	mat4 modelMatrix;
    uint materialId;
    mat3 normalTransform;
};

layout(binding = 1, set = UPDATE_PER_FRAME, scalar) readonly buffer TSceneData {
    DrawData perDrawData[];
} SceneData;

struct Material {
    vec3 albedo;
    float roughness;
    float metallic;
    int albedoTextureIndex;
    int metallicRoughnessTextureIndex;
    int normalTextureIndex;
};

layout(binding = 0, set = UPDATE_ONCE) uniform sampler textureSampler;
layout(binding = 1, set = UPDATE_ONCE, scalar) readonly buffer TMaterial {
    Material values[];
} Materials;
layout(binding = 2, set = UPDATE_ONCE) uniform texture2D sceneTextures[];

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

void main() {
    const vec3 LightDir = normalize(vec3(0, -1, 1));
    const float LightRadiance = 1.0f;

    DrawData perDrawData = SceneData.perDrawData[inDrawID];

    uint materialId = perDrawData.materialId;
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

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTextureIndex >= 0) {
        vec2 metallicRoughness =
            texture(sampler2D(sceneTextures[material.metallicRoughnessTextureIndex], textureSampler), inTexCoord)
                .rg;
        roughness = metallicRoughness.x;
        metallic = metallicRoughness.y;
    }

    vec3 N = normalize(inNormal);
    vec3 V = -CameraParameters.cameraForwardDir.xyz;
    vec3 L = -LightDir;
    float radiance = LightRadiance;
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    {
        vec3 H = normalize(V + L);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;  
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
