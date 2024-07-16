#pragma once

#include <vector>

#include "Object.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"
#include "MeshSystem.h"

namespace VKRT {

class Context;

class Scene : public RefCountPtr {
public:
    Scene(ScopedRefPtr<Context> context);

    void AddObject(ScopedRefPtr<Object> object);

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
    SceneMaterials GetMaterialProxies();

    ScopedRefPtr<MeshSystem> GetMeshSystem() { return mMeshSystem; }

    void Draw(
        vk::CommandBuffer& commandBuffer,
        std::function<void(vk::CommandBuffer, ScopedRefPtr<Object>, ScopedRefPtr<Mesh>)>
            onDrawMesh);

    ~Scene();

private:
    std::vector<ScopedRefPtr<Object>> GetFlattenedObjects();

    ScopedRefPtr<MeshSystem> mMeshSystem;

    ScopedRefPtr<Context> mContext;

    std::vector<ScopedRefPtr<Object>> mObjects;

    ScopedRefPtr<Texture> mDummyTexture;
};
}  // namespace VKRT