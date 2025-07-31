#pragma once

#include <vector>

#include "Camera.h"
#include "MeshSystem.h"
#include "Object.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"
#include "DirectionalLight.h"

namespace VKRT {

class Context;

class Scene : public RefCountPtr {
public:
    Scene(ScopedRefPtr<Context> context);

    void AddObject(ScopedRefPtr<Object> object);

    void Lock();

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
    const SceneMaterials& GetMaterialProxies() const { return mCachedMaterialProxies; }
    const std::vector<ScopedRefPtr<Object>>& GetFlattenedObjects() const {
        return mCachedFlattenedObjects;
    }
    const uint32_t GetDrawCallCount() { return mCachedDrawCallCount; }
    ScopedRefPtr<MeshSystem> GetMeshSystem() { return mMeshSystem; }
    DirectionalLight& GetLight() { return mLight; }
    void Update();

    ~Scene();

private:
    void FlattenedObjects();
    void GenerateMaterialProxies();

    ScopedRefPtr<MeshSystem> mMeshSystem;

    ScopedRefPtr<Context> mContext;

    std::vector<ScopedRefPtr<Object>> mObjects;
    DirectionalLight mLight;

    ScopedRefPtr<Texture> mDummyTexture;

    SceneMaterials mCachedMaterialProxies;

    std::vector<ScopedRefPtr<Object>> mCachedFlattenedObjects;
    uint32_t mCachedDrawCallCount;
};
}  // namespace VKRT