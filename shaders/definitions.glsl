#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1

struct VkDrawIndexedIndirectCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int  vertexOffset;
	uint firstInstance;
};

struct DrawData {
	uint indexCount;
	uint firstIndex;
	int  vertexOffset;
	mat4 modelMatrix;
    uint materialId;
    mat3 normalTransform;
};

struct Material {
    vec3 albedo;
    float roughness;
    float metallic;
    int albedoTextureIndex;
    int metallicRoughnessTextureIndex;
    int normalTextureIndex;
};

