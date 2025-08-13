#ifndef DEFINITIONS_GLSL
#define DEFINITIONS_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1
#define PRIMITIVE_ID_NONE 0xFFFFFFFF

struct VkDrawIndexedIndirectCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int  vertexOffset;
	uint firstInstance;
};

struct AABB {
	vec3 minBounds;
	vec3 maxBounds;
};

struct DrawData {
	uint indexCount;
	uint firstIndex;
	int  vertexOffset;
	mat4 modelMatrix;
    uint materialId;
    mat3 normalTransform;
	uint alphaMode;
	AABB aabb;
};

struct Material {
    vec3 albedo;
    float roughness;
    float metallic;
    int albedoTextureIndex;
    int metallicRoughnessTextureIndex;
    int normalTextureIndex;
};

#endif