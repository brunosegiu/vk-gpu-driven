#ifndef SHADING_GLSL
#define SHADING_GLSL

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

const int POISSON_COUNT = 128;
vec2 PoissonDisk[POISSON_COUNT] = vec2[](
    vec2(0.463950, -0.072217),
    vec2(0.873447, 0.328072),
    vec2(-0.083342, -0.135162),
    vec2(0.837134, -0.201763),
    vec2(0.399359, -0.203975),
    vec2(0.420568, 0.507870),
    vec2(-0.933714, -0.327447),
    vec2(0.354999, 0.636699),
    vec2(-0.952000, -0.145185),
    vec2(0.636811, 0.267208),
    vec2(-0.250689, -0.034403),
    vec2(-0.531285, 0.520922),
    vec2(0.394145, 0.316890),
    vec2(-0.050332, -0.460732),
    vec2(-0.266527, -0.162523),
    vec2(0.208583, 0.786823),
    vec2(0.330637, 0.770085),
    vec2(0.459687, 0.199686),
    vec2(-0.807819, 0.381083),
    vec2(0.243111, 0.092907),
    vec2(-0.086536, 0.725275),
    vec2(0.432886, -0.473724),
    vec2(0.029512, -0.591716),
    vec2(-0.237883, 0.529008),
    vec2(-0.279862, 0.873769),
    vec2(0.904836, 0.169577),
    vec2(0.013942, 0.456781),
    vec2(0.023294, -0.203300),
    vec2(0.708394, -0.173733),
    vec2(-0.791436, 0.062926),
    vec2(-0.683657, 0.297490),
    vec2(0.664615, 0.384819),
    vec2(0.717602, 0.633124),
    vec2(0.442272, 0.818125),
    vec2(-0.150162, -0.529377),
    vec2(-0.396460, -0.615305),
    vec2(-0.249581, -0.351954),
    vec2(0.125342, 0.525869),
    vec2(-0.202660, 0.407241),
    vec2(0.586036, -0.004064),
    vec2(-0.328154, -0.772418),
    vec2(0.547203, 0.633825),
    vec2(-0.827418, -0.144402),
    vec2(0.288351, 0.917260),
    vec2(-0.585752, -0.374866),
    vec2(0.094248, -0.459266),
    vec2(-0.059797, -0.719893),
    vec2(-0.374681, -0.065091),
    vec2(-0.594135, -0.710866),
    vec2(-0.113841, 0.522939),
    vec2(-0.426051, -0.484136),
    vec2(0.303777, 0.413730),
    vec2(0.757911, 0.295382),
    vec2(0.106465, 0.652080),
    vec2(-0.040063, 0.036883),
    vec2(-0.124691, 0.958402),
    vec2(0.789759, -0.354200),
    vec2(0.413981, -0.614822),
    vec2(-0.362793, 0.410704),
    vec2(-0.457774, -0.761141),
    vec2(0.308040, -0.938485),
    vec2(0.105435, -0.301842),
    vec2(0.782316, 0.052307),
    vec2(-0.733695, 0.645872),
    vec2(-0.800922, -0.313832),
    vec2(0.515100, 0.394894),
    vec2(-0.647523, -0.483487),
    vec2(-0.602576, 0.051006),
    vec2(0.183973, -0.199694),
    vec2(0.559598, -0.222504),
    vec2(0.076483, -0.955823),
    vec2(-0.116548, -0.310254),
    vec2(-0.623206, 0.435265),
    vec2(0.956393, -0.176420),
    vec2(-0.527719, -0.249874),
    vec2(0.148648, 0.174815),
    vec2(0.003177, 0.968611),
    vec2(-0.593391, 0.643277),
    vec2(-0.895292, -0.010037),
    vec2(-0.425110, -0.174591),
    vec2(0.782480, -0.479671),
    vec2(-0.390184, 0.714252),
    vec2(0.769153, -0.069662),
    vec2(-0.892868, 0.130841),
    vec2(-0.128702, -0.958197),
    vec2(-0.502431, 0.803503),
    vec2(-0.719728, 0.170135),
    vec2(-0.481983, 0.370516),
    vec2(-0.164075, 0.827580),
    vec2(0.624875, -0.392163),
    vec2(-0.406483, 0.552382),
    vec2(0.586604, 0.120911),
    vec2(0.121087, 0.053796),
    vec2(0.078769, 0.775506),
    vec2(0.410648, -0.803028),
    vec2(-0.251958, -0.633401),
    vec2(0.565661, 0.511328),
    vec2(0.714130, 0.161823),
    vec2(-0.294239, -0.519349),
    vec2(0.234483, -0.351186),
    vec2(-0.026107, 0.611284),
    vec2(0.304677, -0.471806),
    vec2(-0.185989, -0.836835),
    vec2(0.309804, 0.194758),
    vec2(-0.714554, -0.211801),
    vec2(-0.283137, 0.304971),
    vec2(-0.527640, -0.563437),
    vec2(0.114715, -0.082553),
    vec2(0.504744, -0.694830),
    vec2(0.148950, -0.605136),
    vec2(0.277631, -0.062108),
    vec2(0.087475, -0.742822),
    vec2(0.126621, 0.973289),
    vec2(-0.895429, 0.291341),
    vec2(0.464596, 0.050376),
    vec2(0.646404, -0.693754),
    vec2(-0.354784, -0.290730),
    vec2(0.716724, 0.505963),
    vec2(0.013120, 0.183694),
    vec2(0.455234, -0.349966),
    vec2(0.175133, -0.882887),
    vec2(0.182700, 0.412590),
    vec2(-0.749631, 0.495588),
    vec2(0.220303, 0.600763),
    vec2(0.539542, -0.812102),
    vec2(-0.435937, 0.059413),
    vec2(-0.458118, 0.241060),
    vec2(0.625794, -0.547308)
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

vec3 evalLighting(vec3 N, vec3 V, vec3 L, vec3 radiance, float shadowTerm, vec4 albedo, float metallic, float roughness, float visibility) {
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);
    {
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
    }
    
    vec3 ambient = radiance * vec3(0.1) * albedo.rgb * visibility;

    // Hack to get some reflections on metallic materials while there are no reflection probes
    ProceduralSkyShaderParameters params = initSkyShaderParameters(L);
    params.lightColor = normalize(radiance);
    vec3 skySpecular = fakeSkyReflectionFast(N, V, roughness, F0, params);

    return ambient + Lo + skySpecular;
}

vec3 gammaCorrection(vec3 color) {
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0/2.2));
}

#endif