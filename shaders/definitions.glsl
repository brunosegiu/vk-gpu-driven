#ifndef DEFINITIONS_GLSL
#define DEFINITIONS_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_scalar_block_layout : enable

#define UPDATE_PER_FRAME 0
#define UPDATE_ONCE 1
#define PRIMITIVE_ID_NONE 0xFFFFFFFF

const float PI = 3.14159265359;
const float PHI = 2.399963229728653;
const float EPSILON = 1e-5;
const float INF = 1e15;

struct VkDrawIndexedIndirectCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int  vertexOffset;
	uint firstInstance;
};

struct DrawData {
    uint meshIndex;
	uint indexCount;
	uint firstIndex;
	int vertexOffset;
    uint alphaMode;
	vec3 minBounds;
	vec3 maxBounds;
    vec3 coneApex;
    vec3 coneAxis;
    float coneCutoff;
};

struct MeshData {
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

struct CameraData {
    mat4 projection;
    mat4 viewProjection;
	mat4 invViewProjection;
	mat4 invProjection;
    mat4 invView;
    vec4 cameraPos;
};

struct SSAOControlData {
    float radius;
    float power;
    uint kernelSize;
    int blurRadius;
};

struct LightData {
    vec3 radiance;
    vec3 direction;
    mat4 viewProjection;
    uint shadowTaps;
};

const int ColorPayloadIndex = 0;
const int ShadowPayloadIndex = 1;

const int ColorMissIndex = 0;
const int ShadowMissIndex = 1;

const float TMin = 0.001f;
const float TMax = 1000.0f;
const float Infinity = TMax * 100.0f;
const uint DefaultSBTOffset = 0;
const uint DefaultSBTStride = 0;
const uint MaxRecursionLevel = 4;
const uint AllMask = 0xFF;

struct RayPayload {
    vec3 color;
    float hitDepth;
    uint depth;
};

struct DDGIData {
    uvec3 probeGridCount;
    vec3 probeGridOrigin;
    vec3 probeSpacing;
    float minRayLength;
    float maxRayLength;
    mat3 randomRotation;
    float hysteresis;
    uint frameIndex;
};

#endif