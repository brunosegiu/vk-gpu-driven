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

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

const mat4 ShadowBiasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

// From Sascha Willem's examples
float textureProj(vec4 shadowCoord, vec2 offset, sampler shadowSampler, texture2D shadowMap) {
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
        float shadowDepth = texture(sampler2D(shadowMap, shadowSampler), shadowCoord.st + offset ).r;
		if (shadowCoord.w > 0.0 && shadowDepth < shadowCoord.z) {
			shadow = 0.0f;
		}
	}
	return shadow;
}

// https://github.com/GPUOpen-Effects/ShadowFX/blob/master/amd_shadowfx/src/Shaders/AMD_SHADOWFX_FILTER_SIZE_15_POISSON.inc
const int POISSON_COUNT = 51;
const vec2 PoissonDisk[POISSON_COUNT] = vec2[](
  vec2(0.272153, 0.147179),
  vec2(0.282677, 0.277615),
  vec2(0.111645, 0.268837),
  vec2(0.400319, 0.047563),
  vec2(0.408985, 0.194408),
  vec2(0.534191, 0.030173),
  vec2(0.556673, 0.135536),
  vec2(0.334234, 0.402806),
  vec2(0.146173, 0.166826),
  vec2(0.199067, 0.360825),
  vec2(0.437338, 0.328581),
  vec2(0.699799, 0.089076),
  vec2(0.044831, 0.445143),
  vec2(0.087581, 0.557190),
  vec2(0.183368, 0.600518),
  vec2(0.174441, 0.496664),
  vec2(0.356468, 0.686440),
  vec2(0.234973, 0.715005),
  vec2(0.280913, 0.525297),
  vec2(0.081819, 0.713120),
  vec2(0.437340, 0.572705),
  vec2(0.688016, 0.193690),
  vec2(0.807257, 0.105998),
  vec2(0.321767, 0.802520),
  vec2(0.271984, 0.914389),
  vec2(0.154543, 0.802825),
  vec2(0.538678, 0.378504),
  vec2(0.525553, 0.233833),
  vec2(0.518190, 0.774127),
  vec2(0.519246, 0.651308),
  vec2(0.408107, 0.867631),
  vec2(0.818168, 0.233048),
  vec2(0.441349, 0.451439),
  vec2(0.666632, 0.454801),
  vec2(0.578477, 0.556066),
  vec2(0.702981, 0.323121),
  vec2(0.665180, 0.678351),
  vec2(0.930022, 0.288821),
  vec2(0.828204, 0.422613),
  vec2(0.948008, 0.414735),
  vec2(0.804097, 0.772157),
  vec2(0.720558, 0.849087),
  vec2(0.770057, 0.628413),
  vec2(0.594638, 0.852394),
  vec2(0.934051, 0.562872),
  vec2(0.868392, 0.686201),
  vec2(0.689193, 0.560781),
  vec2(0.677183, 0.947311),
  vec2(0.561896, 0.966587),
  vec2(0.823353, 0.881428),
  vec2(0.827946, 0.541060)
);

float filterPCF(vec4 sc, uint taps, sampler shadowSampler, texture2D shadowMap) {
	ivec2 texDim = textureSize(sampler2D(shadowMap, shadowSampler), 0);
	vec2 texelSize = 1.2 / vec2(texDim);

    uint samples = clamp(taps, 1, POISSON_COUNT);
	float shadowFactor = 0.0;
	for (int i = 0; i < samples; ++i) {
		shadowFactor += textureProj(sc, texelSize * PoissonDisk[i], shadowSampler, shadowMap);
	}
	return shadowFactor / float(samples);
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
    parameters.groundColor = vec3(0.62F, 0.59F, 0.55F);
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

vec3 fakeSkyReflectionFast(vec3 N, vec3 V, float roughness, vec3 F0, ProceduralSkyShaderParameters params)
{
    vec3  R = reflect(-V, N);
    vec3 specularLight = getProceduralSkyColor(params, R, 0);
    specularLight *= (roughness < 0.2) ? (1.0 - roughness) * (1.0 - roughness) : 0;
    specularLight *= 0.1f;
    return specularLight;
}

vec3 evalLighting(vec3 N, vec3 V, vec3 L, vec3 radiance, float shadowTerm, vec4 albedo, float metallic, float roughness, float visibility, vec3 indirect, vec3 emissive) {
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);
    vec3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;  
            
    float NdotL = max(dot(N, L), 0.0);
        
    Lo = (kD * albedo.rgb / PI + specular) * NdotL * radiance * shadowTerm;

    vec3 ambient = indirect * albedo.rgb * visibility;

    return ambient + Lo + emissive;
}

vec3 gammaCorrection(vec3 color) {
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0/2.2));
}

#endif