#ifndef SHADING_GLSL
#define SHADING_GLSL

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

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

const mat4 ShadowBiasMat = mat4(
    vec4(0.5, 0.0, 0.0, 0.0),
    vec4(0.0, 0.5, 0.0, 0.0),
    vec4(0.0, 0.0, 1.0, 0.0),
    vec4(0.5, 0.5, 0.0, 1.0)
);

float filterESM(vec4 shadowCoord, float expScale, sampler shadowSampler, texture2D shadowMap) {	
    vec2 shadowUv = shadowCoord.xy;
    if (any(lessThan(shadowUv, vec2(0.0))) ||
        any(greaterThan(shadowUv, vec2(1.0)))) {
        return 1.0;
    }
    float moment = texture(sampler2D(shadowMap, shadowSampler), shadowUv).r;
    float visibility = moment * exp(-expScale * shadowCoord.z);
    const float bleedK = 0.2;
    return clamp((visibility - bleedK) / (1.0 - bleedK), 0.0, 1.0);
}

// From: https://github.com/nvpro-samples/nvpro_core/blob/master/nvvkhl/shaders/dh_sky.h
struct ProceduralSkyShaderParameters {
    vec3 directionToLight;
    float angularSizeOfLight;
    vec3 lightColor;
    float glowSize;
    vec3 skyColor;
    float glowIntensity;
    vec3 horizonColor;
    float horizonSize;
    vec3 groundColor;
    float glowSharpness;
    vec3 directionUp;
    float pad1;
};

ProceduralSkyShaderParameters initSkyShaderParameters(vec3 directionToLight) {
    ProceduralSkyShaderParameters parameters;
    parameters.directionToLight = directionToLight;
    parameters.angularSizeOfLight = 0.059F;
    parameters.lightColor = vec3(1.0F, 1.0F, 1.0F);
    parameters.skyColor = vec3(0.17F, 0.37F, 0.65F);
    parameters.horizonColor = vec3(0.50F, 0.70F, 0.92F);
    parameters.groundColor = vec3(0.0F, 0.0F, 0.0F);
    parameters.directionUp = vec3(0.F, 1.F, 0.F);
    parameters.horizonSize = 0.5F;
    parameters.glowSize = 0.091F;
    parameters.glowIntensity = 0.9F;
    parameters.glowSharpness = 4.F;

    return parameters;
}

vec3 getProceduralSkyColor(
    ProceduralSkyShaderParameters params,
    vec3 direction,
    float angularSizeOfPixel) {
    float elevation = asin(clamp(dot(direction, params.directionUp), -1.0F, 1.0F));
    float top = smoothstep(0.F, params.horizonSize, elevation);
    float bottom = smoothstep(0.F, params.horizonSize, -elevation);
    vec3 environment =
        mix(mix(params.horizonColor, params.groundColor, bottom), params.skyColor, top);

    float angleToLight = acos(clamp(dot(direction, params.directionToLight), 0.0F, 1.0F));
    float halfAngularSize = params.angularSizeOfLight * 0.5F;
    float lightIntensity = clamp(
        1.0F - smoothstep(
                   halfAngularSize - angularSizeOfPixel * 2.0F,
                   halfAngularSize + angularSizeOfPixel * 2.0F,
                   angleToLight),
        0.0F,
        1.0F);
    lightIntensity = pow(lightIntensity, 4.0F);
    float glow_input = clamp(
        2.0F * (1.0F - smoothstep(
                           halfAngularSize - params.glowSize,
                           halfAngularSize + params.glowSize,
                           angleToLight)),
        0.0F,
        1.0F);
    float glow_intensity = params.glowIntensity * pow(glow_input, params.glowSharpness);
    vec3 light = max(lightIntensity, glow_intensity) * params.lightColor;

    return environment + light;
}

vec3 fakeSkyReflectionFast(vec3 N, vec3 V, float roughness, vec3 F0, ProceduralSkyShaderParameters params) {
    vec3  R = reflect(-V, N);
    vec3 specularLight = getProceduralSkyColor(params, R, 0);
    specularLight *= (roughness < 0.2) ? (1.0 - roughness) * (1.0 - roughness) : 0;
    specularLight *= 0.1f;
    return specularLight;
}

struct ShadingParams {
    vec3 N;
    vec3 V;
    vec3 L;
    vec3 radiance;
    float shadowTerm;
    vec4 albedo;
    float metallic;
    float roughness;
    vec3 emissive;
    float visibility; // AO
    vec3 indirectDiffuse;
    vec3 indirectGlossy;
    float directWeight;
    float indirectDiffuseWeight;
    float indirectGlossyWeight;
};

vec3 evalLighting(ShadingParams params) {
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, params.albedo.rgb, params.metallic);

    vec3 Lo = vec3(0.0);
    vec3 H = normalize(params.V + params.L);

    float NDF = DistributionGGX(params.N, H, params.roughness);
    float G = GeometrySmith(params.N, params.V, params.L, params.roughness);
    vec3 F = FresnelSchlick(max(dot(H, params.V), 0.0), F0);
        
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - params.metallic);
        
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(params.N, params.V), 0.0) * max(dot(params.N, params.L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;  
            
    float NdotL = max(dot(params.N, params.L), 0.0);
        
    Lo = (kD * params.albedo.rgb / PI + specular) * NdotL * params.radiance * params.shadowTerm;

    vec3 ambient = params.indirectDiffuse * params.albedo.rgb * params.visibility;

    return Lo * params.directWeight + params.emissive + ambient * params.indirectDiffuseWeight + kS * params.indirectGlossy * params.indirectGlossyWeight;
}

vec3 gammaCorrection(vec3 color) {
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0/2.2));
}

#endif