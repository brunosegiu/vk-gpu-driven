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

float filterPCF(vec4 sc, sampler shadowSampler, texture2D shadowMap) {
	const int samples = 12;
	const vec2 poissonDisk[samples] = vec2[](
        vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
        vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893),
        vec2(0.507, 0.064), vec2(0.896, 0.412), vec2(-0.322, 0.949), vec2(-0.609, 0.347)
    );

	ivec2 texDim = textureSize(sampler2D(shadowMap, shadowSampler), 0);
	vec2 texelSize = 1.2 / vec2(texDim);

	float shadowFactor = 0.0;
	for (int i = 0; i < samples; ++i) {
		shadowFactor += textureProj(sc, texelSize * poissonDisk[i], shadowSampler, shadowMap);
	}
	return shadowFactor / float(samples);
}