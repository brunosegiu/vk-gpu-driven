#pragma once

#include <vector>

#include "MeshSystem.h"
#include "Object.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"
#include "Camera.h"

namespace VKRT {

class Context;

class Scene : public RefCountPtr {
public:
    Scene(ScopedRefPtr<Context> context);

    void AddObject(ScopedRefPtr<Object> object);

    void Lock();

    struct MaterialProxy {
        glm::vec3 albedo;
        glm::vec3 emissive;
        float roughness;
        float metallic;
        float transmission;
        float indexOfRefraction;
        int32_t albedoTextureIndex;
        int32_t roughnessTextureIndex;
    };
    struct SceneMaterials {
        std::vector<MaterialProxy> materials;
        std::vector<ScopedRefPtr<Texture>> textures;
    };
    const SceneMaterials& GetMaterialProxies() const { return mCachedMaterialProxies; }
    std::vector<ScopedRefPtr<Object>> GetFlattenedObjects() const {
        return mCachedFlattenedObjects;
    }
    ScopedRefPtr<MeshSystem> GetMeshSystem() { return mMeshSystem; }

    void Draw(
        vk::CommandBuffer& commandBuffer,
        ScopedRefPtr<Camera> camera,
        std::function<void(vk::CommandBuffer, ScopedRefPtr<Object>, ScopedRefPtr<Mesh>)>
            onDrawMesh);

    ~Scene();

private:
    void FlattenedObjects();
    void GenerateMaterialProxies();


    ScopedRefPtr<MeshSystem> mMeshSystem;

    ScopedRefPtr<Context> mContext;

    std::vector<ScopedRefPtr<Object>> mObjects;

    ScopedRefPtr<Texture> mDummyTexture;

    SceneMaterials mCachedMaterialProxies;

    std::vector<ScopedRefPtr<Object>> mCachedFlattenedObjects;
};
}  // namespace VKRT