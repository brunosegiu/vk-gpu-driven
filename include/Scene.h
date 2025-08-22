#pragma once

#include <vector>

#include "Camera.h"
#include "DirectionalLight.h"
#include "MeshSystem.h"
#include "Object.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Context;

class Scene : public RefCountPtr {
public:
    Scene(ScopedRefPtr<Context> context);

    void Load(std::string path);

    void Update();

    struct MaterialProxy {
        glm::vec3 albedo;
        float roughness;
        float metallic;
        int32_t albedoTextureIndex;
        int32_t metallicRoughnessTextureIndex;
        int32_t normalTextureIndex;
    };
    struct SceneMaterials {
        std::vector<MaterialProxy> materials;
        std::vector<ScopedRefPtr<Texture>> textures;
    };

    struct PersistentDrawData {
        uint32_t meshIndex;
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
        uint32_t alphaMode;
        glm::vec3 minBounds;
        glm::vec3 maxBounds;
        glm::vec3 coneApex;
        glm::vec3 coneAxis;
        float coneCutoff;
    };

    struct MeshData {
        glm::mat4 transform;
        uint32_t materialId;
        glm::mat3 normalTransform;
    };

    struct PackedDrawData {
        std::vector<PersistentDrawData> persistentDrawData;
        std::vector<MeshData> perMeshData;
    };
    const PackedDrawData& GetPackedDrawData() const { return mPackedDrawData; }
    const SceneMaterials& GetMaterialProxies() const { return mSceneMaterials; }
    ScopedRefPtr<MeshSystem> GetMeshSystem() { return mMeshSystem; }
    const uint32_t GetDrawCallCount(Material::AlphaMode alphaMode) {
        return mRenderPassResources[alphaMode].cachedDrawCallCount;
    }
    const uint32_t GetDrawCallOffset(Material::AlphaMode alphaMode) {
        return mRenderPassResources[alphaMode].cachedDrawOffset;
    }
    DirectionalLight& GetLight() { return mLight; }

    const vk::AccelerationStructureKHR& GetTLAS() { return mRaytracingScene.mTLAS; }

    ~Scene();

private:
    void PackDrawData();

    ScopedRefPtr<Context> mContext;
    std::vector<ScopedRefPtr<Object>> mObjects;
    std::vector<ScopedRefPtr<Object>> mFlatObjects;
    DirectionalLight mLight;

    ScopedRefPtr<Texture> mDummyTexture;

    ScopedRefPtr<MeshSystem> mMeshSystem;
    PackedDrawData mPackedDrawData;
    std::vector<ScopedRefPtr<Material>> mMaterials;
    std::vector<ScopedRefPtr<Mesh>> mMeshes;
    SceneMaterials mSceneMaterials;
    struct RenderPassResources {
        uint32_t cachedDrawOffset;
        uint32_t cachedDrawCallCount;
    };
    std::unordered_map<Material::AlphaMode, RenderPassResources> mRenderPassResources;

    struct BlasResources {
        vk::AccelerationStructureKHR mBLAS;
        vk::DeviceAddress mBLASAddress;
    };
    struct RaytracingScene {
        std::vector<BlasResources> blasResources;
        ScopedRefPtr<VulkanBuffer> mBLASBuffer;
        ScopedRefPtr<VulkanBuffer> mInstancesBuffer;
        ScopedRefPtr<VulkanBuffer> mTLASBuffer;
        vk::AccelerationStructureKHR mTLAS;
        vk::DeviceAddress mTLASAddress;
    } mRaytracingScene;
};
}  // namespace VKRT